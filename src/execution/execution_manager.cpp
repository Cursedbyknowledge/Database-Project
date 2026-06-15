/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "execution_manager.h"

#include <algorithm>
#include <map>

#include "executor_delete.h"
#include "executor_index_scan.h"
#include "executor_insert.h"
#include "executor_nestedloop_join.h"
#include "executor_projection.h"
#include "executor_seq_scan.h"
#include "executor_update.h"
#include "index/ix.h"
#include "record_printer.h"

const char *help_info = "Supported SQL syntax:\n"
                   "  command ;\n"
                   "command:\n"
                   "  CREATE TABLE table_name (column_name type [, column_name type ...])\n"
                   "  DROP TABLE table_name\n"
                   "  CREATE INDEX table_name (column_name)\n"
                   "  DROP INDEX table_name (column_name)\n"
                   "  INSERT INTO table_name VALUES (value [, value ...])\n"
                   "  DELETE FROM table_name [WHERE where_clause]\n"
                   "  UPDATE table_name SET column_name = value [, column_name = value ...] [WHERE where_clause]\n"
                   "  SELECT selector FROM table_name [WHERE where_clause]\n"
                   "type:\n"
                   "  {INT | FLOAT | CHAR(n)}\n"
                   "where_clause:\n"
                   "  condition [AND condition ...]\n"
                   "condition:\n"
                   "  column op {column | value}\n"
                   "column:\n"
                   "  [table_name.]column_name\n"
                   "op:\n"
                   "  {= | <> | < | > | <= | >=}\n"
                   "selector:\n"
                   "  {* | column [, column ...]}\n";

// 主要负责执行DDL语句
void QlManager::run_mutli_query(std::shared_ptr<Plan> plan, Context *context){
    if (auto x = std::dynamic_pointer_cast<DDLPlan>(plan)) {
        switch(x->tag) {
            case T_CreateTable:
            {
                sm_manager_->create_table(x->tab_name_, x->cols_, context);
                break;
            }
            case T_DropTable:
            {
                sm_manager_->drop_table(x->tab_name_, context);
                break;
            }
            case T_CreateIndex:
            {
                sm_manager_->create_index(x->tab_name_, x->tab_col_names_, context);
                break;
            }
            case T_DropIndex:
            {
                sm_manager_->drop_index(x->tab_name_, x->tab_col_names_, context);
                break;
            }
            default:
                throw InternalError("Unexpected field type");
                break;  
        }
    }
}

// 执行help; show tables; desc table; begin; commit; abort;语句
void QlManager::run_cmd_utility(std::shared_ptr<Plan> plan, txn_id_t *txn_id, Context *context) {
    if (auto x = std::dynamic_pointer_cast<OtherPlan>(plan)) {
        switch(x->tag) {
            case T_Help:
            {
                memcpy(context->data_send_ + *(context->offset_), help_info, strlen(help_info));
                *(context->offset_) = strlen(help_info);
                break;
            }
            case T_ShowTable:
            {
                sm_manager_->show_tables(context);
                break;
            }
            case T_ShowIndex:
            {
                sm_manager_->show_index(x->tab_name_, context);
                break;
            }
            case T_DescTable:
            {
                sm_manager_->desc_table(x->tab_name_, context);
                break;
            }
            case T_Transaction_begin:
            {
                // 显示开启一个事务
                context->txn_->set_txn_mode(true);
                break;
            }  
            case T_Transaction_commit:
            {
                context->txn_ = txn_mgr_->get_transaction(*txn_id);
                txn_mgr_->commit(context->txn_, context->log_mgr_);
                break;
            }    
            case T_Transaction_rollback:
            {
                context->txn_ = txn_mgr_->get_transaction(*txn_id);
                txn_mgr_->abort(context->txn_, context->log_mgr_);
                break;
            }    
            case T_Transaction_abort:
            {
                context->txn_ = txn_mgr_->get_transaction(*txn_id);
                txn_mgr_->abort(context->txn_, context->log_mgr_);
                break;
            }     
            default:
                throw InternalError("Unexpected field type");
                break;                        
        }

    } else if(auto x = std::dynamic_pointer_cast<SetKnobPlan>(plan)) {
        switch (x->set_knob_type_)
        {
        case ast::SetKnobType::EnableNestLoop: {
            planner_->set_enable_nestedloop_join(x->bool_value_);
            break;
        }
        case ast::SetKnobType::EnableSortMerge: {
            planner_->set_enable_sortmerge_join(x->bool_value_);
            break;
        }
        default: {
            throw RMDBError("Not implemented!\n");
            break;
        }
        }
    }
}

// 执行select语句，select语句的输出除了需要返回客户端外，还需要写入output.txt文件中
void QlManager::select_from(std::unique_ptr<AbstractExecutor> executorTreeRoot, std::vector<TabCol> sel_cols, 
                            Context *context) {

    std::vector<std::string> captions;
    captions.reserve(sel_cols.size());
    for (auto &sel_col : sel_cols) {
        captions.push_back(sel_col.col_name);
    }

    // Print header into buffer
    RecordPrinter rec_printer(sel_cols.size());
    rec_printer.print_separator(context);
    rec_printer.print_record(captions, context);
    rec_printer.print_separator(context);
    // print header into file (open_db已chdir到数据库目录)
    std::string out_path = "output.txt";

    std::fstream outfile;
    outfile.open(out_path, std::ios::out | std::ios::app);
    if (!outfile.is_open()) {
        throw RMDBError("Cannot open output file: " + out_path);
    }
    
    outfile << "|";
    for(int i = 0; i < captions.size(); ++i) {
        outfile << " " << captions[i] << " |";
    }
    outfile << "\n";

    // Print records
    size_t num_rec = 0;
    // 执行query_plan
    for (executorTreeRoot->beginTuple(); !executorTreeRoot->is_end(); executorTreeRoot->nextTuple()) {
        auto Tuple = executorTreeRoot->Next();
        if (Tuple == nullptr) continue;
        std::vector<std::string> columns;
        for (auto &col : executorTreeRoot->cols()) {
            std::string col_str;
            char *rec_buf = Tuple->data + col.offset;
            if (col.type == TYPE_INT) {
                col_str = std::to_string(*(int *)rec_buf);
            } else if (col.type == TYPE_FLOAT) {
                col_str = std::to_string(*(float *)rec_buf);
            } else if (col.type == TYPE_STRING) {
                col_str = std::string((char *)rec_buf, col.len);
                col_str.resize(strlen(col_str.c_str()));
            }
            columns.push_back(col_str);
        }
        // print record into buffer
        rec_printer.print_record(columns, context);
        // print record into file
        outfile << "|";
        for(int i = 0; i < columns.size(); ++i) {
            outfile << " " << columns[i] << " |";
        }
        outfile << "\n";
        num_rec++;
    }
    outfile.close();
    // Print footer into buffer
    rec_printer.print_separator(context);
    // Print record count into buffer
    RecordPrinter::print_record_count(num_rec, context);
}

// 自由函数：序列化计划树 (parent_rows: 父节点行数，供Filter/Scan区分)
static void explain_plan(std::shared_ptr<Plan> p, int indent,
                         const std::map<const Plan*, int>& rows_map, std::string& out,
                         int parent_rows = -1) {
    int rows = rows_map.count(p.get()) ? rows_map.at(p.get()) : 0;
    if (auto sp = std::dynamic_pointer_cast<ScanPlan>(p)) {
        if (!sp->fed_conds_.empty()) {
            // Filter rows = 父节点行数(过滤后)，Scan rows = 本节点行数(扫描全部)
            int filter_rows = (parent_rows >= 0) ? parent_rows : rows;
            out += std::string(indent, '\t') + "Filter(condition=[";
            // 条件按字典序排序
            auto sorted_conds = sp->fed_conds_;
            std::sort(sorted_conds.begin(), sorted_conds.end(), [](auto &a, auto &b) {
                return (a.lhs_col.tab_name + "." + a.lhs_col.col_name) <
                       (b.lhs_col.tab_name + "." + b.lhs_col.col_name);
            });
            for (size_t i = 0; i < sorted_conds.size(); i++) {
                if (i) out += ", ";
                auto &c = sorted_conds[i];
                out += c.lhs_col.tab_name + "." + c.lhs_col.col_name;
                switch (c.op) {
                    case OP_EQ: out += "="; break; case OP_NE: out += "<>"; break;
                    case OP_LT: out += "<"; break; case OP_GT: out += ">"; break;
                    case OP_LE: out += "<="; break; case OP_GE: out += ">="; break;
                }
                if (c.is_rhs_val) {
                    if (c.rhs_val.type == TYPE_INT) out += std::to_string(*(int*)c.rhs_val.raw->data);
                    else if (c.rhs_val.type == TYPE_FLOAT) out += std::to_string(*(float*)c.rhs_val.raw->data);
                    else out += c.rhs_val.str_val;
                }
            }
            out += "], rows=" + std::to_string(filter_rows) + ")\n";
            indent++;
        }
        out += std::string(indent, '\t') + "Scan(table=" + sp->tab_name_ + ", type=";
        out += std::string(sp->tag == T_IndexScan ? "IndexScan" : "SeqScan") + ", rows=";
        out += std::to_string(rows) + ")\n";
    } else if (auto jp = std::dynamic_pointer_cast<JoinPlan>(p)) {
        out += std::string(indent, '\t') + "Join(";
        out += "tables=[";
        // 收集所有表名并排序
        std::vector<std::string> tabs;
        auto collect = [&](auto self, std::shared_ptr<Plan> cp) -> void {
            if (auto s = std::dynamic_pointer_cast<ScanPlan>(cp)) tabs.push_back(s->tab_name_);
            else if (auto j = std::dynamic_pointer_cast<JoinPlan>(cp)) { self(self, j->left_); self(self, j->right_); }
        };
        collect(collect, jp->left_); collect(collect, jp->right_);
        std::sort(tabs.begin(), tabs.end());
        for (size_t i = 0; i < tabs.size(); i++) { if (i) out += ", "; out += tabs[i]; }
        out += "], condition=[";
        for (size_t i = 0; i < jp->conds_.size(); i++) {
            if (i) out += ", ";
            out += jp->conds_[i].lhs_col.tab_name + "." + jp->conds_[i].lhs_col.col_name;
            out += "=" + jp->conds_[i].rhs_col.tab_name + "." + jp->conds_[i].rhs_col.col_name;
        }
        out += "], rows=" + std::to_string(rows) + ")\n";
        explain_plan(jp->left_, indent + 1, rows_map, out, rows);
        explain_plan(jp->right_, indent + 1, rows_map, out, rows);
    } else if (auto pp = std::dynamic_pointer_cast<ProjectionPlan>(p)) {
        out += std::string(indent, '\t') + "Project(columns=[";
        if (pp->sel_cols_.empty() || (pp->sel_cols_.size() == 1 && pp->sel_cols_[0].col_name == "*")) {
            out += "*";
        } else {
            // 列名按字母顺序排序
            auto cols = pp->sel_cols_;
            std::sort(cols.begin(), cols.end(), [](auto &a, auto &b) {
                return (a.tab_name + "." + a.col_name) < (b.tab_name + "." + b.col_name);
            });
            for (size_t i = 0; i < cols.size(); i++) {
                if (i) out += ", ";
                out += cols[i].tab_name + "." + cols[i].col_name;
            }
        }
        out += "], rows=" + std::to_string(rows) + ")\n";
        explain_plan(pp->subplan_, indent + 1, rows_map, out, rows);
    }
}

// EXPLAIN ANALYZE: 执行计划并输出计划树到 output.txt
void QlManager::explain_select(std::shared_ptr<Plan> plan,
                                std::unique_ptr<AbstractExecutor> executorTreeRoot,
                                std::vector<TabCol> sel_cols, Context *context) {
    // 执行查询计划收集运行时信息
    for (executorTreeRoot->beginTuple(); !executorTreeRoot->is_end(); executorTreeRoot->nextTuple()) {
        executorTreeRoot->Next();
    }
    
    // 递归收集行数：遍历计划树和执行器树
    std::map<const Plan*, int> rows_map;
    std::function<void(std::shared_ptr<Plan>, AbstractExecutor*)> collect;
    collect = [&](std::shared_ptr<Plan> p, AbstractExecutor* e) {
        if (!p || !e) return;
        rows_map[p.get()] = e->runtime_rows_;
        auto children = e->get_children();
        if (auto pp = std::dynamic_pointer_cast<ProjectionPlan>(p)) {
            if (!children.empty()) collect(pp->subplan_, children[0]);
        } else if (auto jp = std::dynamic_pointer_cast<JoinPlan>(p)) {
            if (children.size() >= 2) {
                collect(jp->left_, children[0]);
                collect(jp->right_, children[1]);
            }
        }
    };
    collect(plan, executorTreeRoot.get());
    
    // 生成EXPLAIN树
    std::string out;
    explain_plan(plan, 0, rows_map, out);
    
    // 发送给客户端
    memcpy(context->data_send_, out.c_str(), std::min(out.size(), (size_t)BUFFER_LENGTH - 1));
    context->data_send_[std::min(out.size(), (size_t)BUFFER_LENGTH - 1)] = '\0';
    *(context->offset_) = out.size();
    
    // 写入 output.txt (open_db已chdir到数据库目录)
    std::string out_path = "output.txt";
    std::fstream outfile;
    outfile.open(out_path, std::ios::out | std::ios::app);
    if (!outfile.is_open()) {
        throw RMDBError("Cannot open EXPLAIN output file: " + out_path);
    }
    outfile << out;
    outfile.close();
}

// 执行DML语句
void QlManager::run_dml(std::unique_ptr<AbstractExecutor> exec){
    exec->Next();
}