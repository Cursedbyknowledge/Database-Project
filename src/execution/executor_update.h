/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. */

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
    std::vector<Rid>::iterator rid_iter_;

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

    void beginTuple() override {
        rid_iter_ = rids_.begin();
    }

    void nextTuple() override {
        if (rid_iter_ != rids_.end()) ++rid_iter_;
    }

    bool is_end() const override { return rid_iter_ == rids_.end(); }
    size_t tupleLen() const override { return 0; }
    const std::vector<ColMeta> &cols() const override { return tab_.cols; }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        Rid rid = *rid_iter_;
        auto old_rec = fh_->get_record(rid, context_);
        
        // Create new record buffer (copy old values first)
        std::unique_ptr<RmRecord> new_rec = std::make_unique<RmRecord>(old_rec->size);
        memcpy(new_rec->data, old_rec->data, old_rec->size);
        
        // Delete old index entries before modifying the record
        for (size_t i = 0; i < tab_.indexes.size(); ++i) {
            auto& index = tab_.indexes[i];
            std::string ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols);
            if (sm_manager_->ihs_.count(ix_name)) {
                auto ih = sm_manager_->ihs_.at(ix_name).get();
                char* old_key = new char[index.col_tot_len];
                int offset = 0;
                for (size_t j = 0; j < (size_t)index.col_num; ++j) {
                    memcpy(old_key + offset, old_rec->data + index.cols[j].offset, index.cols[j].len);
                    offset += index.cols[j].len;
                }
                ih->delete_entry(old_key, context_->txn_);
                delete[] old_key;
            }
        }
        
        // Apply set clauses
        for (auto &clause : set_clauses_) {
            auto pos = std::find_if(tab_.cols.begin(), tab_.cols.end(), [&](const ColMeta &col) {
                return col.name == clause.lhs.col_name;
            });
            if (pos == tab_.cols.end()) continue;
            
            int offset = pos->offset;
            if (pos->type == TYPE_INT) {
                *(int *)(new_rec->data + offset) = clause.rhs.int_val;
            } else if (pos->type == TYPE_FLOAT) {
                *(float *)(new_rec->data + offset) = clause.rhs.float_val;
            } else if (pos->type == TYPE_STRING) {
                memset(new_rec->data + offset, 0, pos->len);
                memcpy(new_rec->data + offset, clause.rhs.str_val.c_str(),
                       std::min((size_t)pos->len, clause.rhs.str_val.size()));
            }
        }
        
        fh_->update_record(rid, new_rec->data, context_);
        
        // Record write for transaction rollback
        if (context_->txn_ != nullptr) {
            WriteRecord *wr = new WriteRecord(WType::UPDATE_TUPLE, tab_name_, rid, *old_rec);
            context_->txn_->append_write_record(wr);
        }
        
        // Insert new index entries
        for (size_t i = 0; i < tab_.indexes.size(); ++i) {
            auto& index = tab_.indexes[i];
            std::string ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index.cols);
            if (sm_manager_->ihs_.count(ix_name)) {
                auto ih = sm_manager_->ihs_.at(ix_name).get();
                char* new_key = new char[index.col_tot_len];
                int offset = 0;
                for (size_t j = 0; j < (size_t)index.col_num; ++j) {
                    memcpy(new_key + offset, new_rec->data + index.cols[j].offset, index.cols[j].len);
                    offset += index.cols[j].len;
                }
                ih->insert_entry(new_key, rid, context_->txn_);
                delete[] new_key;
            }
        }
        
        return nullptr;
    }

    Rid &rid() override { return _abstract_rid; }
};
