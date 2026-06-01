/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class UpdateExecutor : public AbstractExecutor {
   private:
    TabMeta tab_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<Rid> rids_;
    std::string tab_name_;
    std::vector<SetClause> set_clauses_;
    SmManager *sm_manager_;

    // 为一条索引构建键（从记录数据中提取各列值拼接）
    void build_index_key(char *key_buf, const IndexMeta &index, const char *rec_data) {
        int offset = 0;
        for (size_t j = 0; j < index.col_num; ++j) {
            memcpy(key_buf + offset, rec_data + index.cols[j].offset, index.cols[j].len);
            offset += index.cols[j].len;
        }
    }

   public:
    UpdateExecutor(SmManager *sm_manager, const std::string &tab_name, std::vector<SetClause> set_clauses,
                   std::vector<Condition> conds, std::vector<Rid> rids, Context *context) {
        sm_manager_ = sm_manager;
        tab_name_ = tab_name;
        set_clauses_ = set_clauses;
        tab_ = sm_manager_->db_.get_table(tab_name);
        fh_ = sm_manager_->fhs_.at(tab_name).get();
        conds_ = conds;
        rids_ = rids;
        context_ = context;
    }

    std::unique_ptr<RmRecord> Next() override {
        for (auto &rid : rids_) {
            auto rec = fh_->get_record(rid, context_);

            // ---- 阶段 1: 在内存中构建更新后的记录，但不写入磁盘 ----
            // 先拷贝一份原记录数据，用于后续在内存中模拟更新
            std::vector<char> new_data(fh_->get_file_hdr().record_size);
            memcpy(new_data.data(), rec->data, fh_->get_file_hdr().record_size);

            for (auto &set_clause : set_clauses_) {
                auto col_meta = tab_.get_col(set_clause.lhs.col_name);
                set_clause.rhs.init_raw(col_meta->len);
                memcpy(new_data.data() + col_meta->offset, set_clause.rhs.raw->data, col_meta->len);
                if (set_clause.rhs.raw) set_clause.rhs.raw.reset();
            }

            // ---- 阶段 2: 检查唯一性约束（在删除旧条目之前） ----
            for (auto &index : tab_.indexes) {
                std::string ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols);
                auto ih = sm_manager_->ihs_.at(ix_name).get();

                char *new_key = new char[index.col_tot_len];
                build_index_key(new_key, index, new_data.data());

                // 检查是否有其他记录使用了相同键值
                std::vector<Rid> existing;
                if (ih->get_value(new_key, &existing, context_->txn_)) {
                    bool conflict = false;
                    for (auto &ex_rid : existing) {
                        if (ex_rid.page_no != rid.page_no || ex_rid.slot_no != rid.slot_no) {
                            conflict = true;
                            break;
                        }
                    }
                    if (conflict) {
                        delete[] new_key;
                        throw RMDBError("Duplicate entry for unique index");
                    }
                }
                delete[] new_key;
            }

            // ---- 阶段 3: 删除旧的索引条目 ----
            for (auto &index : tab_.indexes) {
                std::string ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols);
                auto ih = sm_manager_->ihs_.at(ix_name).get();

                char *old_key = new char[index.col_tot_len];
                build_index_key(old_key, index, rec->data);
                ih->delete_entry(old_key, context_->txn_);
                delete[] old_key;
            }

            // ---- 阶段 4: 插入新的索引条目 ----
            for (auto &index : tab_.indexes) {
                std::string ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols);
                auto ih = sm_manager_->ihs_.at(ix_name).get();

                char *new_key = new char[index.col_tot_len];
                build_index_key(new_key, index, new_data.data());
                ih->insert_entry(new_key, rid, context_->txn_);
                delete[] new_key;
            }

            // ---- 阶段 5: 写入更新后的记录 ----
            fh_->update_record(rid, new_data.data(), context_);
        }
        return nullptr;
    }

    Rid &rid() override { return _abstract_rid; }
};
