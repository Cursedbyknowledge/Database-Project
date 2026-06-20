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
#include "execution_common.h"

class UpdateExecutor : public AbstractExecutor {
   private:
    TabMeta tab_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<Rid> rids_;
    std::string tab_name_;
    std::vector<SetClause> set_clauses_;
    SmManager *sm_manager_;

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
        for (auto& rid : rids_) {
            // 获取记录并进行条件过滤
            auto rec = fh_->get_record(rid, context_);
            if (rec == nullptr) continue;
            
            // 检查是否满足WHERE条件
            bool match = true;
            for (auto& cond : conds_) {
                if (!eval_cond(rec->data, cond, tab_.cols)) {
                    match = false;
                    break;
                }
            }
            if (!match) continue;
            
            // 预先检查索引唯一性：计算新键值，若已存在则拒绝更新
            bool can_update = true;
            for (auto& index : tab_.indexes) {
                auto ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols);
                auto ih = sm_manager_->ihs_.at(ix_name).get();
                // 计算旧键和新键
                char* old_key = new char[index.col_tot_len];
                char* new_key = new char[index.col_tot_len];
                int off = 0;
                for (int j = 0; j < index.col_num; ++j) {
                    memcpy(old_key + off, rec->data + index.cols[j].offset, index.cols[j].len);
                    // 新值：如果是SET子句中的列，用新值；否则用旧值
                    bool found_in_set = false;
                    for (auto& clause : set_clauses_) {
                        if (clause.lhs.col_name == index.cols[j].name) {
                            memcpy(new_key + off, clause.rhs.raw->data, index.cols[j].len);
                            found_in_set = true;
                            break;
                        }
                    }
                    if (!found_in_set) {
                        memcpy(new_key + off, rec->data + index.cols[j].offset, index.cols[j].len);
                    }
                    off += index.cols[j].len;
                }
                // 如果新旧键不同，检查新键是否已存在
                if (memcmp(old_key, new_key, index.col_tot_len) != 0) {
                    try {
                        auto [dup_leaf, _] = ih->find_leaf_page(new_key, Operation::INSERT, context_ ? context_->txn_ : nullptr);
                        int dup_pos = dup_leaf->lower_bound(new_key);
                        std::vector<ColType> types;
                        std::vector<int> lens;
                        for (auto &c : index.cols) { types.push_back(c.type); lens.push_back(c.len); }
                        if (dup_pos < dup_leaf->get_size() &&
                            ix_compare(dup_leaf->get_key(dup_pos), new_key, types, lens) == 0) {
                            sm_manager_->get_bpm()->unpin_page(dup_leaf->get_page_id(), false);
                            can_update = false;
                        } else {
                            sm_manager_->get_bpm()->unpin_page(dup_leaf->get_page_id(), false);
                        }
                    } catch (IndexEntryNotFoundError&) {
                        // 索引为空，无冲突
                    }
                }
                delete[] old_key; delete[] new_key;
                if (!can_update) break;
            }
            if (!can_update) throw DuplicateIndexError(tab_name_, "");  // 抛异常→Portal写failure
            
            // Save old record for transaction rollback
            RmRecord old_rec(rec->size, rec->data);
            
            // 从所有索引中删除旧条目
            for (auto& index : tab_.indexes) {
                auto ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols);
                auto ih = sm_manager_->ihs_.at(ix_name).get();
                char* old_key = new char[index.col_tot_len];
                int offset = 0;
                for (int j = 0; j < index.col_num; ++j) {
                    memcpy(old_key + offset, rec->data + index.cols[j].offset, index.cols[j].len);
                    offset += index.cols[j].len;
                }
                ih->delete_entry(old_key, context_ ? context_->txn_ : nullptr);
                delete[] old_key;
            }
            
            // 更新记录数据
            for (auto& clause : set_clauses_) {
                auto col_meta = tab_.get_col(clause.lhs.col_name);
                assert(col_meta != tab_.cols.end());
                memcpy(rec->data + col_meta->offset, clause.rhs.raw->data, col_meta->len);
            }
            fh_->update_record(rid, rec->data, context_);
            
            // 在所有索引中插入新条目
            for (auto& index : tab_.indexes) {
                auto ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols);
                auto ih = sm_manager_->ihs_.at(ix_name).get();
                char* new_key = new char[index.col_tot_len];
                int offset = 0;
                for (int j = 0; j < index.col_num; ++j) {
                    memcpy(new_key + offset, rec->data + index.cols[j].offset, index.cols[j].len);
                    offset += index.cols[j].len;
                }
                ih->insert_entry(new_key, rid, context_ ? context_->txn_ : nullptr);
                delete[] new_key;
            }
            // Record write operation for transaction rollback
            context_->txn_->append_write_record(new WriteRecord(WType::UPDATE_TUPLE, tab_name_, rid, old_rec));
        }
        return nullptr;
    }

    Rid &rid() override { return _abstract_rid; }
};
