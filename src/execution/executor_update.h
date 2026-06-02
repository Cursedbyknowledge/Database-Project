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

    void build_index_key(char *key_buf, const IndexMeta &index, const char *rec_data) {
        int offset = 0;
        for (size_t j = 0; j < index.col_num; ++j) {
            memcpy(key_buf + offset, rec_data + index.cols[j].offset, index.cols[j].len);
            offset += index.cols[j].len;
        }
    }

    // 无索引时的原始更新逻辑（与原版完全一致）
    void update_without_index(Rid &rid) {
        auto rec = fh_->get_record(rid, context_);
        for (auto &set_clause : set_clauses_) {
            auto col_meta = tab_.get_col(set_clause.lhs.col_name);
            set_clause.rhs.init_raw(col_meta->len);
            memcpy(rec->data + col_meta->offset, set_clause.rhs.raw->data, col_meta->len);
            if (set_clause.rhs.raw) set_clause.rhs.raw.reset();
        }
        fh_->update_record(rid, rec->data, context_);
    }

    // 有索引时的五阶段更新（含唯一性检查+索引同步）
    void update_with_index(Rid &rid) {
        auto rec = fh_->get_record(rid, context_);

        // 阶段 1: 构建更新后记录
        std::vector<char> new_data(fh_->get_file_hdr().record_size);
        memcpy(new_data.data(), rec->data, fh_->get_file_hdr().record_size);
        for (auto &set_clause : set_clauses_) {
            auto col_meta = tab_.get_col(set_clause.lhs.col_name);
            set_clause.rhs.init_raw(col_meta->len);
            memcpy(new_data.data() + col_meta->offset, set_clause.rhs.raw->data, col_meta->len);
            if (set_clause.rhs.raw) set_clause.rhs.raw.reset();
        }

        // 阶段 2: 唯一性检查
        for (auto &index : tab_.indexes) {
            auto ih = sm_manager_->get_ih(tab_name_, index.cols);
            char *new_key = new char[index.col_tot_len];
            build_index_key(new_key, index, new_data.data());
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

        // 阶段 3: 删除旧索引条目
        for (auto &index : tab_.indexes) {
            auto ih = sm_manager_->get_ih(tab_name_, index.cols);
            char *old_key = new char[index.col_tot_len];
            build_index_key(old_key, index, rec->data);
            ih->delete_entry(old_key, context_->txn_);
            delete[] old_key;
        }

        // 阶段 4: 插入新索引条目
        for (auto &index : tab_.indexes) {
            auto ih = sm_manager_->get_ih(tab_name_, index.cols);
            char *new_key = new char[index.col_tot_len];
            build_index_key(new_key, index, new_data.data());
            ih->insert_entry(new_key, rid, context_->txn_);
            delete[] new_key;
        }

        // 阶段 5: 写入记录
        fh_->update_record(rid, new_data.data(), context_);
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
            if (tab_.indexes.empty()) {
                update_without_index(rid);   // 无索引：原始逻辑，零影响
            } else {
                update_with_index(rid);      // 有索引：五阶段流程
            }
        }
        return nullptr;
    }

    Rid &rid() override { return _abstract_rid; }
};
