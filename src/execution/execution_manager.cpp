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
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
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

// 统一输出拦截器：双保险路径策略
static void write_to_output(SmManager* sm_manager, const std::string& output_str) {
    if (output_str.empty()) return;
    
    std::string db_name = sm_manager->get_db_name();
    std::string path1 = db_name.empty() ? "output.txt" : (db_name + "/output.txt");
    std::string path2 = "output.txt";
    
    std::fstream outfile;
    outfile.open(path1, std::ios::out | std::ios::app);
    if (!outfile.is_open()) {
        outfile.open(path2, std::ios::out | std::ios::app);
    }
    
    if (outfile.is_open()) {
        outfile << output_str;
        outfile.close();
    }
}

// 主要负责执行DDL语句
void QlManager::run_mutli_query(std::shared_ptr<Plan> plan, Context *context){
    int old_offset = *(context->offset_);
    if (auto x = std::dynamic_pointer_cast<DDLPlan>(plan)) {
        switch(x->tag) {
            case T_CreateTable:
                sm_manager_->create_table(x->tab_name_, x->cols_, context); break;
            case T_DropTable:
                sm_manager_->drop_table(x->tab_name_, context); break;
            case T_CreateIndex:
                sm_manager_->create_index(x->tab_name_, x->tab_col_names_, context); break;
            case T_DropIndex:
                sm_manager_->drop_index(x->tab_name_, x->tab_col_names_, context); break;
            default:
                throw InternalError("Unexpected field type"); break;  
        }
    }
    int new_offset = *(context->offset_);
    if (new_offset > old_offset) {
        write_to_output(sm_manager_, std::string(context->data_send_ + old_offset, new_offset - old_offset));
    }
}

// 执行help; show tables; desc table; begin; commit; abort;语句
void QlManager::run_cmd_utility(std::shared_ptr<Plan> plan, txn_id_t *txn_id, Context *context) {
    int old_offset = *(context->offset_);

    if (auto x = std::dynamic_pointer_cast<OtherPlan>(plan)) {
        switch(x->tag) {
            case T_Help:
                memcpy(context->data_send_ + *(context->offset_), help_info, strlen(help_info));
                *(context->offset_) += strlen(help_info);
                break;
            case T_ShowTable:
                sm_manager_->show_tables(context); break;
            case T_ShowIndex:
                sm_manager_->show_index(x->tab_name_, context); break;
            case T_DescTable:
                sm_manager_->desc_table(x->tab_name_, context); break;
            case T_Transaction_begin:
                context->txn_->set_txn_mode(true); break;
            case T_Transaction_commit:
                context->txn_ = txn_mgr_->get_transaction(*txn_id);
                txn_mgr_->commit(context->txn_, context->log_mgr_); break;
            case T_Transaction_rollback:
            case T_Transaction_abort:
                context->txn_ = txn_mgr_->get_transaction(*txn_id);
                txn_mgr_->abort(context->txn_, context->log_mgr_); break;
            default:
                throw InternalError("Unexpected field type"); break;                        
        }
    } else if(auto x = std::dynamic_pointer_cast<SetKnobPlan>(plan)) {
        switch (x->set_knob_type_) {
        case ast::SetKnobType::EnableNestLoop: 
            planner_->set_enable_nestedloop_join(x->bool_value_); break;
        case ast::SetKnobType::EnableSortMerge: 
            planner_->set_enable_sortmerge_join(x->bool_value_); break;
        default: 
            throw RMDBError("Not implemented!\n"); break;
        }
    }

    int new_offset = *(context->offset_);
    if (new_offset > old_offset) {
        write_to_output(sm_manager_, std::string(context->data_send_ + old_offset, new_offset - old_offset));
    }
}

// 执行select语句
void QlManager::select_from(std::unique_ptr<AbstractExecutor> executorTreeRoot, std::vector<TabCol> sel_cols, 
                            Context *context) {
    // 绝对信任根算子Schema：ProjectionExecutor已完美重排/裁剪列，offset准确对齐Tuple内存
    auto &cols = executorTreeRoot->cols();
    std::vector<std::string> captions;
    for (auto &c : cols) captions.push_back(c.name);

    // 客户端输出（RecordPrinter格式，带边框）
    RecordPrinter rec_printer(captions.size());
    rec_printer.print_separator(context);
    rec_printer.print_record(captions, context);
    rec_printer.print_separator(context);

    // output.txt 纯净表格输出
    std::string out = "|";
    for (auto &cap : captions) out += " " + cap + " |";
    out += "\n";

    size_t num_rec = 0;
    for (executorTreeRoot->beginTuple(); !executorTreeRoot->is_end(); executorTreeRoot->nextTuple()) {
        auto Tuple = executorTreeRoot->Next();
        if (Tuple == nullptr) continue;
        std::vector<std::string> columns;
        out += "|";
        for (auto &col : cols) {
            std::string col_str;
            const char *rec_buf = Tuple->data + col.offset;
            if (col.type == TYPE_INT)
                col_str = std::to_string(*(int *)rec_buf);
            else if (col.type == TYPE_FLOAT)
                col_str = std::to_string(*(float *)rec_buf);
            else if (col.type == TYPE_STRING) {
                int len = 0;
                while (len < col.len && rec_buf[len] != '\0') len++;
                col_str = std::string(rec_buf, len);
            }
            columns.push_back(col_str);
            out += " " + col_str + " |";
        }
        out += "\n";
        rec_printer.print_record(columns, context);
        num_rec++;
        if (num_rec > 100000) break;
    }
    rec_printer.print_separator(context);
    RecordPrinter::print_record_count(num_rec, context);

    write_to_output(sm_manager_, out);
}

// 自由函数：序列化计划树
static void explain_plan(std::shared_ptr<Plan> p, int indent,
                         const std::map<const Plan*, int>& rows_map,
                         const std::map<const Plan*, int>& out_rows_map,
                         std::string& out,
                         const std::map<std::string, std::string>& alias_map) {
    int rows = rows_map.count(p.get()) ? rows_map.at(p.get()) : 0;
    int out_rows = out_rows_map.count(p.get()) ? out_rows_map.at(p.get()) : rows;
    // 别名查找：若有别名则用别名，否则用原表名
    auto alias_for = [&](const std::string& real) -> std::string {
        auto it = alias_map.find(real);
        return (it != alias_map.end()) ? it->second : real;
    };
    
    if (auto sp = std::dynamic_pointer_cast<ScanPlan>(p)) {
        if (!sp->fed_conds_.empty()) {
            out += std::string(indent, '\t') + "Filter(condition=[";
            auto sorted_conds = sp->fed_conds_;
            std::sort(sorted_conds.begin(), sorted_conds.end(), [](auto &a, auto &b) {
                return (a.lhs_col.tab_name + "." + a.lhs_col.col_name) <
                       (b.lhs_col.tab_name + "." + b.lhs_col.col_name);
            });
            for (size_t i = 0; i < sorted_conds.size(); i++) {
                if (i) out += ", ";
                auto &c = sorted_conds[i];
                out += alias_for(c.lhs_col.tab_name) + "." + c.lhs_col.col_name;
                switch (c.op) {
                    case OP_EQ: out += "="; break; case OP_NE: out += "<>"; break;
                    case OP_LT: out += "<"; break; case OP_GT: out += ">"; break;
                    case OP_LE: out += "<="; break; case OP_GE: out += ">="; break;
                }
                if (c.is_rhs_val) {
                    if (c.rhs_val.type == TYPE_INT) out += std::to_string(*(int*)c.rhs_val.raw->data);
                    else if (c.rhs_val.type == TYPE_FLOAT) {
                        float fv = *(float*)c.rhs_val.raw->data;
                        std::ostringstream oss;
                        oss << std::fixed << std::setprecision(6) << fv;
                        std::string s = oss.str();
                        // 去除尾随零
                        s.erase(s.find_last_not_of('0') + 1, std::string::npos);
                        if (s.back() == '.') s.pop_back();
                        out += s;
                    }
                    else out += c.rhs_val.str_val;
                }
            }
            out += "], rows=" + std::to_string(out_rows) + ")\n";
            indent++;
        }
        out += std::string(indent, '\t') + "Scan(table=" + sp->tab_name_ + ", type=";
        if (sp->tag == T_IndexScan) {
            out += "IndexScan, using_index=(";
            auto& idx_cols = sp->index_col_names_;
            for (size_t i = 0; i < idx_cols.size(); i++) {
                if (i) out += ", ";
                out += idx_cols[i];
            }
            out += "), rows=";
        } else {
            out += "SeqScan, rows=";
        }
        out += std::to_string(rows) + ")\n";
    } else if (auto jp = std::dynamic_pointer_cast<JoinPlan>(p)) {
        out += std::string(indent, '\t') + "Join(";
        out += "tables=[";
        std::vector<std::string> tabs;
        auto collect = [&](auto self, std::shared_ptr<Plan> cp) -> void {
            if (auto s = std::dynamic_pointer_cast<ScanPlan>(cp)) tabs.push_back(s->tab_name_);
            else if (auto pp2 = std::dynamic_pointer_cast<ProjectionPlan>(cp)) { self(self, pp2->subplan_); }
            else if (auto j = std::dynamic_pointer_cast<JoinPlan>(cp)) { self(self, j->left_); self(self, j->right_); }
        };
        collect(collect, jp->left_); collect(collect, jp->right_);
        std::sort(tabs.begin(), tabs.end());
        for (size_t i = 0; i < tabs.size(); i++) { if (i) out += ", "; out += tabs[i]; }
        out += "], condition=[";
        // Join条件按字典序排序
        auto sorted_join_conds = jp->conds_;
        std::sort(sorted_join_conds.begin(), sorted_join_conds.end(), [](auto &a, auto &b) {
            return (a.lhs_col.tab_name + "." + a.lhs_col.col_name) <
                   (b.lhs_col.tab_name + "." + b.lhs_col.col_name);
        });
        for (size_t i = 0; i < sorted_join_conds.size(); i++) {
            if (i) out += ", ";
            out += alias_for(sorted_join_conds[i].lhs_col.tab_name) + "." + sorted_join_conds[i].lhs_col.col_name;
            out += "=" + alias_for(sorted_join_conds[i].rhs_col.tab_name) + "." + sorted_join_conds[i].rhs_col.col_name;
        }
        out += "], rows=" + std::to_string(rows) + ")\n";
        explain_plan(jp->left_, indent + 1, rows_map, out_rows_map, out, alias_map);
        explain_plan(jp->right_, indent + 1, rows_map, out_rows_map, out, alias_map);
    } else if (auto pp = std::dynamic_pointer_cast<ProjectionPlan>(p)) {
        out += std::string(indent, '\t') + "Project(columns=[";
        if (pp->is_star_ || pp->sel_cols_.empty() || (pp->sel_cols_.size() == 1 && pp->sel_cols_[0].col_name == "*")) {
            out += "*";
        } else {
            auto cols = pp->sel_cols_;
            std::sort(cols.begin(), cols.end(), [](auto &a, auto &b) {
                return (a.tab_name + "." + a.col_name) < (b.tab_name + "." + b.col_name);
            });
            for (size_t i = 0; i < cols.size(); i++) {
                if (i) out += ", ";
                out += alias_for(cols[i].tab_name) + "." + cols[i].col_name;
            }
        }
        out += "], rows=" + std::to_string(rows) + ")\n";
        explain_plan(pp->subplan_, indent + 1, rows_map, out_rows_map, out, alias_map);
    }
}

// EXPLAIN ANALYZE: 执行计划并输出计划树
void QlManager::explain_select(std::shared_ptr<Plan> plan,
                                std::unique_ptr<AbstractExecutor> executorTreeRoot,
                                std::vector<TabCol> sel_cols, Context *context) {
    int old_offset = *(context->offset_);

    int safety = 0;
    for (executorTreeRoot->beginTuple(); !executorTreeRoot->is_end(); executorTreeRoot->nextTuple()) {
        executorTreeRoot->Next();
        if (++safety > 100000) {
            std::cerr << "WARNING: explain_select loop exceeded 100000, breaking" << std::endl;
            break;
        }
    }
    
    std::map<const Plan*, int> rows_map, out_rows_map;
    std::function<void(std::shared_ptr<Plan>, AbstractExecutor*)> collect;
    collect = [&](std::shared_ptr<Plan> p, AbstractExecutor* e) {
        if (!p || !e) return;
        rows_map[p.get()] = e->runtime_rows_;
        out_rows_map[p.get()] = e->runtime_output_;
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
    
    std::string out;
    explain_plan(plan, 0, rows_map, out_rows_map, out, context->rev_alias_map_);
    
    int write_len = std::min(out.size(), (size_t)BUFFER_LENGTH - 1 - old_offset);
    memcpy(context->data_send_ + old_offset, out.c_str(), write_len);
    *(context->offset_) += write_len;
    
    write_to_output(sm_manager_, out);
}

// 执行DML语句
void QlManager::run_dml(std::unique_ptr<AbstractExecutor> exec){
    // 【核心修复】：必须使用循环驱动火山模型，确保多行 Update/Delete 被完全执行！
    for (exec->beginTuple(); !exec->is_end(); exec->nextTuple()) {
        exec->Next();
    }
}
