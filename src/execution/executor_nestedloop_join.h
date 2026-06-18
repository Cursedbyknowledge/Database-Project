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

class NestedLoopJoinExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> left_;
    std::unique_ptr<AbstractExecutor> right_;
    size_t len_;
    std::vector<ColMeta> cols_;

    std::vector<Condition> fed_conds_;
    bool isend;

    std::unique_ptr<RmRecord> left_rec_;
    std::unique_ptr<RmRecord> right_rec_;

    bool eval_join_cond(const Condition &cond) {
        const char *lhs_ptr = nullptr;
        const char *rhs_ptr = nullptr;
        ColType lhs_type = TYPE_INT;

        // 智能列名匹配：如果查询没有指定表名前缀(empty)，则直接按照列名(col_name)匹配
        for (auto &col : cols_) {
            if ((cond.lhs_col.tab_name.empty() || col.tab_name == cond.lhs_col.tab_name) && col.name == cond.lhs_col.col_name) {
                if (col.offset < (int)left_->tupleLen()) {
                    lhs_ptr = left_rec_->data + col.offset;
                } else {
                    lhs_ptr = right_rec_->data + (col.offset - left_->tupleLen());
                }
                lhs_type = col.type;
                break;
            }
        }
        if (!lhs_ptr) return true;

        if (cond.is_rhs_val) {
            if (lhs_type == TYPE_INT) {
                int lhs_val = *(int *)lhs_ptr;
                int rhs_val = cond.rhs_val.int_val;
                switch (cond.op) {
                    case OP_EQ: return lhs_val == rhs_val;
                    case OP_NE: return lhs_val != rhs_val;
                    case OP_LT: return lhs_val < rhs_val;
                    case OP_GT: return lhs_val > rhs_val;
                    case OP_LE: return lhs_val <= rhs_val;
                    case OP_GE: return lhs_val >= rhs_val;
                }
            } else if (lhs_type == TYPE_FLOAT) {
                float lhs_val = *(float *)lhs_ptr;
                float rhs_val = cond.rhs_val.float_val;
                switch (cond.op) {
                    case OP_EQ: return lhs_val == rhs_val;
                    case OP_NE: return lhs_val != rhs_val;
                    case OP_LT: return lhs_val < rhs_val;
                    case OP_GT: return lhs_val > rhs_val;
                    case OP_LE: return lhs_val <= rhs_val;
                    case OP_GE: return lhs_val >= rhs_val;
                }
            } else if (lhs_type == TYPE_STRING) {
                int max_len = 255;
                for (auto &col : cols_) {
                    if ((cond.lhs_col.tab_name.empty() || col.tab_name == cond.lhs_col.tab_name) && col.name == cond.lhs_col.col_name) {
                        max_len = col.len;
                        break;
                    }
                }
                int len = 0;
                while (len < max_len && lhs_ptr[len] != '\0') len++;
                std::string lhs_val(lhs_ptr, len);
                std::string rhs_val = cond.rhs_val.str_val;
                switch (cond.op) {
                    case OP_EQ: return lhs_val == rhs_val;
                    case OP_NE: return lhs_val != rhs_val;
                    case OP_LT: return lhs_val < rhs_val;
                    case OP_GT: return lhs_val > rhs_val;
                    case OP_LE: return lhs_val <= rhs_val;
                    case OP_GE: return lhs_val >= rhs_val;
                }
            }
            return true;
        } else {
            ColType rhs_type = TYPE_INT;
            int rhs_max_len = 255;
            for (auto &col : cols_) {
                if ((cond.rhs_col.tab_name.empty() || col.tab_name == cond.rhs_col.tab_name) && col.name == cond.rhs_col.col_name) {
                    if (col.offset < (int)left_->tupleLen()) {
                        rhs_ptr = left_rec_->data + col.offset;
                    } else {
                        rhs_ptr = right_rec_->data + (col.offset - left_->tupleLen());
                    }
                    rhs_type = col.type;
                    rhs_max_len = col.len;
                    break;
                }
            }
            if (!rhs_ptr) return true;

            if (lhs_type == TYPE_INT) {
                int lhs_val = *(int *)lhs_ptr;
                int rhs_val = *(int *)rhs_ptr;
                switch (cond.op) {
                    case OP_EQ: return lhs_val == rhs_val;
                    case OP_NE: return lhs_val != rhs_val;
                    case OP_LT: return lhs_val < rhs_val;
                    case OP_GT: return lhs_val > rhs_val;
                    case OP_LE: return lhs_val <= rhs_val;
                    case OP_GE: return lhs_val >= rhs_val;
                }
            } else if (lhs_type == TYPE_FLOAT) {
                float lhs_val = *(float *)lhs_ptr;
                float rhs_val = *(float *)rhs_ptr;
                switch (cond.op) {
                    case OP_EQ: return lhs_val == rhs_val;
                    case OP_NE: return lhs_val != rhs_val;
                    case OP_LT: return lhs_val < rhs_val;
                    case OP_GT: return lhs_val > rhs_val;
                    case OP_LE: return lhs_val <= rhs_val;
                    case OP_GE: return lhs_val >= rhs_val;
                }
            } else if (lhs_type == TYPE_STRING) {
                int lhs_max_len = 255;
                for (auto &col : cols_) {
                    if ((cond.lhs_col.tab_name.empty() || col.tab_name == cond.lhs_col.tab_name) && col.name == cond.lhs_col.col_name) {
                        lhs_max_len = col.len; break;
                    }
                }
                int llen = 0; while (llen < lhs_max_len && lhs_ptr[llen] != '\0') llen++;
                int rlen = 0; while (rlen < rhs_max_len && rhs_ptr[rlen] != '\0') rlen++;
                
                std::string lhs_val(lhs_ptr, llen);
                std::string rhs_val(rhs_ptr, rlen);
                switch (cond.op) {
                    case OP_EQ: return lhs_val == rhs_val;
                    case OP_NE: return lhs_val != rhs_val;
                    case OP_LT: return lhs_val < rhs_val;
                    case OP_GT: return lhs_val > rhs_val;
                    case OP_LE: return lhs_val <= rhs_val;
                    case OP_GE: return lhs_val >= rhs_val;
                }
            }
            return true;
        }
    }

    bool eval_conds() {
        for (auto &cond : fed_conds_) {
            if (!eval_join_cond(cond)) return false;
        }
        return true;
    }

    void advance_to_match() {
        int safety = 0;
        while (!left_->is_end()) {
            if (left_rec_ != nullptr) {
                while (!right_->is_end()) {
                    right_rec_ = right_->Next();
                    if (right_rec_ != nullptr && eval_conds()) {
                        return;
                    }
                    right_->nextTuple();
                    if (++safety > 1000000) { isend = true; return; }
                }
            }
            // 右表耗尽，推进左表并重置右表
            left_->nextTuple();
            left_rec_ = nullptr;
            while (!left_->is_end()) {
                left_rec_ = left_->Next();
                if (left_rec_ != nullptr) break;
                left_->nextTuple();
            }
            if (left_rec_ != nullptr) {
                right_->beginTuple();
            }
        }
        isend = true;
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
        isend = false;
        fed_conds_ = std::move(conds);
    }

    void beginTuple() override {
        isend = false;
        left_->beginTuple();
        while (!left_->is_end()) {
            left_rec_ = left_->Next();
            if (left_rec_ != nullptr) break;
            left_->nextTuple();
        }
        if (left_->is_end()) { isend = true; return; }

        right_->beginTuple();
        advance_to_match();
    }

    void nextTuple() override {
        if (isend) return;
        right_->nextTuple();
        advance_to_match();
    }

    bool is_end() const override {
        return isend;
    }

    std::unique_ptr<RmRecord> Next() override {
        if (isend) return nullptr;
        runtime_rows_++;
        runtime_output_++;
        
        auto record = std::make_unique<RmRecord>(len_);
        memcpy(record->data, left_rec_->data, left_->tupleLen());
        memcpy(record->data + left_->tupleLen(), right_rec_->data, right_->tupleLen());
        return record;
    }

    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }
    Rid &rid() override { return _abstract_rid; }
    std::vector<AbstractExecutor*> get_children() override { return {left_.get(), right_.get()}; }
};
