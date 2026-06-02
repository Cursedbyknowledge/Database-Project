/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <netinet/in.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <atomic>

#include "errors.h"
#include "optimizer/optimizer.h"
#include "recovery/log_recovery.h"
#include "optimizer/plan.h"
#include "optimizer/planner.h"
#include "portal.h"
#include "analyze/analyze.h"

#define SOCK_PORT 8765
#define MAX_CONN_LIMIT 8

static bool should_exit = false;

auto disk_manager = std::make_unique<DiskManager>();
auto buffer_pool_manager = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager.get());
auto rm_manager = std::make_unique<RmManager>(disk_manager.get(), buffer_pool_manager.get());
auto ix_manager = std::make_unique<IxManager>(disk_manager.get(), buffer_pool_manager.get());
auto sm_manager = std::make_unique<SmManager>(disk_manager.get(), buffer_pool_manager.get(), rm_manager.get(), ix_manager.get());
auto lock_manager = std::make_unique<LockManager>();
auto txn_manager = std::make_unique<TransactionManager>(lock_manager.get(), sm_manager.get());
auto planner = std::make_unique<Planner>(sm_manager.get());
auto optimizer = std::make_unique<Optimizer>(sm_manager.get(), planner.get());
auto ql_manager = std::make_unique<QlManager>(sm_manager.get(), txn_manager.get(), nullptr);
auto log_manager = std::make_unique<LogManager>(disk_manager.get());
auto recovery = std::make_unique<RecoveryManager>(disk_manager.get(), buffer_pool_manager.get(), sm_manager.get());
auto portal = std::make_unique<Portal>(sm_manager.get());
auto analyze = std::make_unique<Analyze>(sm_manager.get());
pthread_mutex_t *buffer_mutex;
pthread_mutex_t *sockfd_mutex;

static jmp_buf jmpbuf;
void sigint_handler(int signo) {
    should_exit = true;
    log_manager->flush_log_to_disk();
    std::cout << "The Server receive Crtl+C, will been closed\n";
    longjmp(jmpbuf, 1);
}

void SetTransaction(txn_id_t *txn_id, Context *context) {
    context->txn_ = txn_manager->get_transaction(*txn_id);
    if(context->txn_ == nullptr || context->txn_->get_state() == TransactionState::COMMITTED ||
        context->txn_->get_state() == TransactionState::ABORTED) {
        context->txn_ = txn_manager->begin(nullptr, context->log_mgr_);
        *txn_id = context->txn_->get_transaction_id();
        context->txn_->set_txn_mode(false);
    }
}

void *client_handler(void *sock_fd) {
    int fd = *((int *)sock_fd);
    pthread_mutex_unlock(sockfd_mutex);

    char cwd_buf[1024];
    getcwd(cwd_buf, sizeof(cwd_buf));
    if (chdir(sm_manager->db_.name_.c_str()) < 0) {
        std::cerr << "Failed to chdir to database" << std::endl;
    }

    int i_recvBytes;
    char data_recv[BUFFER_LENGTH];
    char *data_send = new char[BUFFER_LENGTH];
    int offset = 0;
    txn_id_t txn_id = INVALID_TXN_ID;

    std::string output = "establish client connection, sockfd: " + std::to_string(fd) + "\n";
    std::cout << output;

    while (true) {
        std::cout << "Waiting for request..." << std::endl;
        memset(data_recv, 0, BUFFER_LENGTH);

        i_recvBytes = read(fd, data_recv, BUFFER_LENGTH);

        if (i_recvBytes == 0) {
            std::cout << "Maybe the client has closed" << std::endl;
            break;
        }
        if (i_recvBytes == -1) {
            std::cout << "Client read error!" << std::endl;
            break;
        }
        
        printf("i_recvBytes: %d \n ", i_recvBytes);

        if (strcmp(data_recv, "exit") == 0) {
            std::cout << "Client exit." << std::endl;
            break;
        }
        if (strcmp(data_recv, "crash") == 0) {
            std::cout << "Server crash" << std::endl;
            exit(1);
        }

        std::cout << "Read from client " << fd << ": " << data_recv << std::endl;

        memset(data_send, '\0', BUFFER_LENGTH);
        offset = 0;

        Context *context = new Context(lock_manager.get(), log_manager.get(), nullptr, data_send, &offset);
        SetTransaction(&txn_id, context);

        bool finish_analyze = false;
        pthread_mutex_lock(buffer_mutex);

        if (strncmp(data_recv, "show index from ", 16) == 0) {
            std::string sql(data_recv);
            size_t start = 16;
            while (start < sql.size() && isspace((unsigned char)sql[start])) start++;
            size_t end = start;
            while (end < sql.size() && !isspace((unsigned char)sql[end]) && sql[end] != ';') end++;
            std::string tab_name = sql.substr(start, end - start);
            pthread_mutex_unlock(buffer_mutex);

            try {
                sm_manager->show_index_from(tab_name, context);
            } catch (RMDBError &e) {
                std::cerr << e.what() << std::endl;
                memcpy(data_send, e.what(), e.get_msg_len());
                data_send[e.get_msg_len()] = '\n';
                data_send[e.get_msg_len() + 1] = '\0';
                offset = e.get_msg_len() + 1;
            }
        } else {
            YY_BUFFER_STATE buf = yy_scan_string(data_recv);
            if (yyparse() == 0) {
                if (ast::parse_tree != nullptr) {
                    try {
                        std::shared_ptr<Query> query = analyze->do_analyze(ast::parse_tree);
                        yy_delete_buffer(buf);
                        finish_analyze = true;
                        pthread_mutex_unlock(buffer_mutex);
                        std::shared_ptr<Plan> plan = optimizer->plan_query(query, context);
                        std::shared_ptr<PortalStmt> portalStmt = portal->start(plan, context);
                        portal->run(portalStmt, ql_manager.get(), &txn_id, context);
                        portal->drop();
                    } catch (TransactionAbortException &e) {
                        std::string str = "abort\n";
                        memcpy(data_send, str.c_str(), str.length());
                        data_send[str.length()] = '\0';
                        offset = str.length();

                        txn_manager->abort(context->txn_, log_manager.get());
                        context->txn_ = nullptr;
                        txn_id = INVALID_TXN_ID;
                        std::cout << e.GetInfo() << std::endl;

                        std::fstream outfile;
                        outfile.open("output.txt", std::ios::out | std::ios::app);
                        outfile << str;
                        outfile.close();
                    } catch (RMDBError &e) {
                        std::cerr << e.what() << std::endl;

                        memcpy(data_send, e.what(), e.get_msg_len());
                        data_send[e.get_msg_len()] = '\n';
                        data_send[e.get_msg_len() + 1] = '\0';
                        offset = e.get_msg_len() + 1;

                        std::fstream outfile;
                        outfile.open("output.txt",std::ios::out | std::ios::app);
                        outfile << "failure\n";
                        outfile.close();
                    }
                }
            }
            if(finish_analyze == false) {
                yy_delete_buffer(buf);
                pthread_mutex_unlock(buffer_mutex);
            }
        }
        if (write(fd, data_send, offset + 1) == -1) {
            break;
        }
        if(context->txn_ != nullptr && context->txn_->get_txn_mode() == false)
        {
            txn_manager->commit(context->txn_, context->log_mgr_);
            txn_id = INVALID_TXN_ID;
        }
    }

    std::cout << "Terminating current client_connection..." << std::endl;
    chdir(cwd_buf);
    close(fd);
    pthread_exit(NULL);
}

void start_server() {
    buffer_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    sockfd_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(buffer_mutex, nullptr);
    pthread_mutex_init(sockfd_mutex, nullptr);

    int sockfd_server;
    int fd_temp;
    struct sockaddr_in s_addr_in {};

    sockfd_server = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd_server != -1);
    int val = 1;
    setsockopt(sockfd_server, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    memset(&s_addr_in, 0, sizeof(s_addr_in));
    s_addr_in.sin_family = AF_INET;
    s_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr_in.sin_port = htons(SOCK_PORT);
    fd_temp = bind(sockfd_server, (struct sockaddr *)(&s_addr_in), sizeof(s_addr_in));
    if (fd_temp == -1) {
        std::cout << "Bind error!" << std::endl;
        exit(1);
    }

    fd_temp = listen(sockfd_server, MAX_CONN_LIMIT);
    if (fd_temp == -1) {
        std::cout << "Listen error!" << std::endl;
        exit(1);
    }

    while (!should_exit) {
        std::cout << "Waiting for new connection..." << std::endl;
        pthread_t thread_id;
        struct sockaddr_in s_addr_client {};
        int client_length = sizeof(s_addr_client);

        if (setjmp(jmpbuf)) {
            std::cout << "Break from Server Listen Loop\n";
            break;
        }

        pthread_mutex_lock(sockfd_mutex);
        int sockfd = accept(sockfd_server, (struct sockaddr *)(&s_addr_client), (socklen_t *)(&client_length));
        if (sockfd == -1) {
            std::cout << "Accept error!" << std::endl;
            continue;
        }
        
        if (pthread_create(&thread_id, nullptr, &client_handler, (void *)(&sockfd)) != 0) {
            std::cout << "Create thread fail!" << std::endl;
            break;
        }

    }

    std::cout << " Try to close all client-connection.\n";
    int ret = shutdown(sockfd_server, SHUT_WR);
    if(ret == -1) { printf("%s\n", strerror(errno)); }
    sm_manager->close_db();
    std::cout << " DB has been closed.\n";
    std::cout << "Server shuts down." << std::endl;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <database>" << std::endl;
        exit(1);
    }

    signal(SIGINT, sigint_handler);
    try {
        std::cout << "\n"
                     "  _____  __  __ _____  ____  \n"
                     " |  __ \\|  \\/  |  __ \\|  _ \\ \n"
                     " | |__) | \\  / | |  | | |_) |\n"
                     " |  _  /| |\\/| | |  | |  _ < \n"
                     " | | \\ \\| |  | | |__| | |_) |\n"
                     " |_|  \\_\\_|  |_|_____/|____/ \n"
                     "\n"
                     "Welcome to RMDB!\n"
                     "Type 'help;' for help.\n"
                     "\n";
        std::string db_name = argv[1];
        if (!sm_manager->is_dir(db_name)) {
            sm_manager->create_db(db_name);
        }
        sm_manager->open_db(db_name);

        // recovery->analyze();  // 注释掉，排查 storage 崩溃
        // recovery->redo();
        // recovery->undo();
        
        start_server();
    } catch (RMDBError &e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }
    return 0;
}