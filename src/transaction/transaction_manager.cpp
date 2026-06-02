/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "transaction_manager.h"
#include "record/rm_file_handle.h"
#include "system/sm_manager.h"

std::unordered_map<txn_id_t, Transaction *> TransactionManager::txn_map = {};

Transaction * TransactionManager::begin(Transaction* txn, LogManager* log_manager) {
    if (txn != nullptr) {
        return txn;
    }
    txn_id_t txn_id = next_txn_id_.fetch_add(1);
    auto *new_txn = new Transaction(txn_id);
    new_txn->set_state(TransactionState::GROWING);
    std::unique_lock<std::mutex> lock(latch_);
    txn_map[txn_id] = new_txn;
    lock.unlock();
    return new_txn;
}

void TransactionManager::commit(Transaction* txn, LogManager* log_manager) {
    txn->set_state(TransactionState::COMMITTED);
    if (log_manager != nullptr) {
        log_manager->flush_log_to_disk();
    }
    std::unique_lock<std::mutex> lock(latch_);
    txn_map.erase(txn->get_transaction_id());
    lock.unlock();
    delete txn;
}

void TransactionManager::abort(Transaction * txn, LogManager *log_manager) {
    txn->set_state(TransactionState::ABORTED);
    if (log_manager != nullptr) {
        log_manager->flush_log_to_disk();
    }
    std::unique_lock<std::mutex> lock(latch_);
    txn_map.erase(txn->get_transaction_id());
    lock.unlock();
    delete txn;
}