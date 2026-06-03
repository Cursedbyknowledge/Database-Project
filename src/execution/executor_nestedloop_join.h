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

class NestedLoopJoinExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> left_;
    std::unique_ptr<AbstractExecutor> right_;
    size_t len_;
    std::vector<ColMeta> cols_;

    std::vector<Condition> fed_conds_;
    bool isend_;
    std::unique_ptr<RmRecord> left_record_;
    std::unique_ptr<RmRecord> right_record_;  // cached right record for Next()

    int val_compare(const char *a, const char *b, ColType type, int len) {
        if (type == TYPE_INT) {
            int ia = *(int *)a, ib = *(int *)b;
            return (ia < ib) ? -1 : ((ia > ib) ? 1 : 0);
        } else if (type == TYPE_FLOAT) {
            float fa = *(float *)a, fb = *(float *)b;
            return (fa < fb) ? -1 : ((fa > fb) ? 1 : 0);
        } else {
            return memcmp(a, b, len);
        }
    }

    // Check if left+right pair satisfies join condition (only EQ supported)
    bool satisfy_join_cond(const RmRecord *left_rec, const RmRecord *right_rec) {
        if (fed_conds_.empty()) return true;
        for (auto &cond : fed_conds_) {
            auto &left_cols = left_->cols();
            auto &right_cols = right_->cols();
            
            char *lhs_ptr = nullptr;
            char *rhs_ptr = nullptr;
            ColType col_type = TYPE_INT;
            int col_len = 0;
            
            auto lpos = std::find_if(left_cols.begin(), left_cols.end(), [&](const ColMeta &c) {
                return c.tab_name == cond.lhs_col.tab_name && c.name == cond.lhs_col.col_name;
            });
            if (lpos != left_cols.end()) {
                col_type = lpos->type;
                col_len = lpos->len;
                lhs_ptr = left_rec->data + lpos->offset;
                auto rpos = std::find_if(right_cols.begin(), right_cols.end(), [&](const ColMeta &c) {
                    return c.tab_name == cond.rhs_col.tab_name && c.name == cond.rhs_col.col_name;
                });
                if (rpos == right_cols.end()) return false;
                rhs_ptr = right_rec->data + rpos->offset;
            } else {
                auto rpos = std::find_if(right_cols.begin(), right_cols.end(), [&](const ColMeta &c) {
                    return c.tab_name == cond.lhs_col.tab_name && c.name == cond.lhs_col.col_name;
                });
                if (rpos == right_cols.end()) return false;
                col_type = rpos->type;
                col_len = rpos->len;
                lhs_ptr = right_rec->data + rpos->offset;
                auto lpos2 = std::find_if(left_cols.begin(), left_cols.end(), [&](const ColMeta &c) {
                    return c.tab_name == cond.rhs_col.tab_name && c.name == cond.rhs_col.col_name;
                });
                if (lpos2 == left_cols.end()) return false;
                rhs_ptr = left_rec->data + lpos2->offset;
            }
            
            int cmp = val_compare(lhs_ptr, rhs_ptr, col_type, col_len);
            if (cmp != 0) return false;
        }
        return true;
    }

    // Find next matching right record for current left, or advance left if exhausted
    // Returns false if no more matches at all
    bool find_next_match() {
        while (true) {
            // Try next right with current left
            right_->nextTuple();
            if (!right_->is_end()) {
                right_record_ = right_->Next();
                if (satisfy_join_cond(left_record_.get(), right_record_.get())) return true;
                continue;
            }
            // Right exhausted, advance left
            left_->nextTuple();
            if (left_->is_end()) {
                isend_ = true;
                return false;
            }
            left_record_ = left_->Next();
            right_->beginTuple();
            if (!right_->is_end()) {
                right_record_ = right_->Next();
                if (satisfy_join_cond(left_record_.get(), right_record_.get())) return true;
                continue;
            }
            // Right is empty
            isend_ = true;
            return false;
        }
    }

   public:
    NestedLoopJoinExecutor(std::unique_ptr<AbstractExecutor> left, std::unique_ptr<AbstractExecutor> right, 
                            std::vector<Condition> conds) {
        left_ = std::move(left);
        right_ = std::move(right);
        len_ = left_->tupleLen() + right_->tupleLen();
        cols_ = left_->cols();
        auto right_cols = right_->cols();
        for (auto &col : right_cols) {
            col.offset += left_->tupleLen();
        }
        cols_.insert(cols_.end(), right_cols.begin(), right_cols.end());
        isend_ = false;
        fed_conds_ = std::move(conds);
    }

    void beginTuple() override {
        isend_ = false;
        left_->beginTuple();
        if (left_->is_end()) { isend_ = true; return; }
        left_record_ = left_->Next();
        
        // Find first matching right record
        right_->beginTuple();
        while (!right_->is_end()) {
            right_record_ = right_->Next();
            if (satisfy_join_cond(left_record_.get(), right_record_.get())) return;
            right_->nextTuple();
        }
        // No match, advance left
        find_next_match();
    }

    void nextTuple() override {
        if (isend_) return;
        find_next_match();
    }

    bool is_end() const override { return isend_; }

    std::unique_ptr<RmRecord> Next() override {
        if (isend_) return nullptr;
        auto result = std::make_unique<RmRecord>(len_);
        memcpy(result->data, left_record_->data, left_->tupleLen());
        memcpy(result->data + left_->tupleLen(), right_record_->data, right_->tupleLen());
        return result;
    }

    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }
    Rid &rid() override { return _abstract_rid; }
};
