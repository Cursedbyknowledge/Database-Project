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

class NestedLoopJoinExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> left_;
    std::unique_ptr<AbstractExecutor> right_;
    size_t len_;
    std::vector<ColMeta> cols_;

    std::vector<Condition> fed_conds_;
    bool isend_;
    std::unique_ptr<RmRecord> left_record_;
    std::unique_ptr<RmRecord> right_record_;

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
        rows_ = 0;
        left_->beginTuple();
    }

    void nextTuple() override {
        if (left_->is_end()) {
            isend_ = true;
            return;
        }
        if (right_->is_end() || !right_record_) {
            right_->beginTuple();
        } else {
            right_->nextTuple();
        }
        while (!left_->is_end()) {
            while (!right_->is_end()) {
                left_record_ = left_->Next();
                right_record_ = right_->Next();
                if (left_record_ && right_record_) {
                    bool match = true;
                    for (auto& cond : fed_conds_) {
                        if (!eval_cond_join(left_record_->data, right_record_->data, cond,
                                           left_->cols(), right_->cols())) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        rows_++;  // 计数连接输出行
                        isend_ = false;
                        return;
                    }
                }
                right_->nextTuple();
            }
            left_->nextTuple();
            if (!left_->is_end()) {
                right_->beginTuple();
            }
        }
        isend_ = true;
    }

    std::unique_ptr<RmRecord> Next() override {
        if (!left_record_ || !right_record_) return nullptr;
        auto joined = std::make_unique<RmRecord>(len_);
        memcpy(joined->data, left_record_->data, left_->tupleLen());
        memcpy(joined->data + left_->tupleLen(), right_record_->data, right_->tupleLen());
        return joined;
    }

    bool is_end() const override { return isend_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }
    size_t tupleLen() const override { return len_; }

    Rid &rid() override { return _abstract_rid; }

    std::vector<AbstractExecutor*> sub_executors() override { return {left_.get(), right_.get()}; }
};
