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
    std::unique_ptr<AbstractExecutor> left_;    // 左儿子节点（需要join的表）
    std::unique_ptr<AbstractExecutor> right_;   // 右儿子节点（需要join的表）
    size_t len_;                                // join后获得的每条记录的长度
    std::vector<ColMeta> cols_;                 // join后获得的记录的字段

    std::vector<Condition> fed_conds_;          // join条件
    bool isend_;
    std::unique_ptr<RmRecord> left_record_;
    bool left_has_more_;

    // Compare two values
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

    // Check if left+right record satisfies join condition
    bool satisfy_join_cond(const RmRecord *left_rec, const RmRecord *right_rec) {
        if (fed_conds_.empty()) return true;
        for (auto &cond : fed_conds_) {
            auto &left_cols = left_->cols();
            auto &right_cols = right_->cols();
            
            ColMeta *col_meta = nullptr;
            char *lhs_ptr = nullptr;
            char *rhs_ptr = nullptr;
            
            // Determine which side has the lhs
            auto lpos = std::find_if(left_cols.begin(), left_cols.end(), [&](const ColMeta &c) {
                return c.tab_name == cond.lhs_col.tab_name && c.name == cond.lhs_col.col_name;
            });
            if (lpos != left_cols.end()) {
                col_meta = &(*lpos);
                lhs_ptr = left_rec->data + lpos->offset;
                auto rpos = std::find_if(right_cols.begin(), right_cols.end(), [&](const ColMeta &c) {
                    return c.tab_name == cond.rhs_col.tab_name && c.name == cond.rhs_col.col_name;
                });
                if (rpos != right_cols.end()) {
                    rhs_ptr = right_rec->data + rpos->offset;
                } else {
                    return false;
                }
            } else {
                auto rpos = std::find_if(right_cols.begin(), right_cols.end(), [&](const ColMeta &c) {
                    return c.tab_name == cond.lhs_col.tab_name && c.name == cond.lhs_col.col_name;
                });
                if (rpos == right_cols.end()) return false;
                col_meta = &(*rpos);
                lhs_ptr = right_rec->data + rpos->offset;
                
                auto lpos2 = std::find_if(left_cols.begin(), left_cols.end(), [&](const ColMeta &c) {
                    return c.tab_name == cond.rhs_col.tab_name && c.name == cond.rhs_col.col_name;
                });
                if (lpos2 == left_cols.end()) return false;
                rhs_ptr = left_rec->data + lpos2->offset;
            }
            
            int cmp = val_compare(lhs_ptr, rhs_ptr, col_meta->type, col_meta->len);
            if (cmp != 0) return false;  // Only support EQ for join conditions
        }
        return true;
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
        left_->beginTuple();
        isend_ = false;
        if (left_->is_end()) {
            isend_ = true;
            return;
        }
        left_record_ = left_->Next();
        // Reset right scan for the new left record
        right_->beginTuple();
        // Find first matching pair
        while (!right_->is_end()) {
            auto right_rec = right_->Next();
            if (satisfy_join_cond(left_record_.get(), right_rec.get())) {
                return;  // Found a match
            }
            right_->nextTuple();
        }
        // No match with current left, advance left
        advance_left();
    }

    // Advance left and reset right
    void advance_left() {
        left_->nextTuple();
        if (left_->is_end()) {
            isend_ = true;
            return;
        }
        left_record_ = left_->Next();
        right_->beginTuple();
        while (!right_->is_end()) {
            auto right_rec = right_->Next();
            if (satisfy_join_cond(left_record_.get(), right_rec.get())) return;
            right_->nextTuple();
        }
        advance_left();  // Recursively try next left
    }

    void nextTuple() override {
        if (isend_) return;
        // Try next right with current left
        right_->nextTuple();
        while (!right_->is_end()) {
            auto right_rec = right_->Next();
            if (satisfy_join_cond(left_record_.get(), right_rec.get())) return;
            right_->nextTuple();
        }
        // No more matches with current left, advance left
        advance_left();
    }

    bool is_end() const override { return isend_; }

    std::unique_ptr<RmRecord> Next() override {
        if (isend_) return nullptr;
        auto right_rec = right_->Next();
        auto result = std::make_unique<RmRecord>(len_);
        memcpy(result->data, left_record_->data, left_->tupleLen());
        memcpy(result->data + left_->tupleLen(), right_rec->data, right_->tupleLen());
        return result;
    }

    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }
    Rid &rid() override { return _abstract_rid; }
};
