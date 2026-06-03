/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. */

#pragma once
#include <algorithm>
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class SortExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> prev_;
    ColMeta cols_;                              // 排序键
    size_t tuple_num_;
    bool is_desc_;
    std::vector<std::pair<std::unique_ptr<RmRecord>, size_t>> sorted_tuples_;  // (record, original_index)
    size_t current_idx_;

   public:
    SortExecutor(std::unique_ptr<AbstractExecutor> prev, TabCol sel_cols, bool is_desc) {
        prev_ = std::move(prev);
        cols_ = prev_->get_col_offset(sel_cols);
        is_desc_ = is_desc;
        tuple_num_ = 0;
        current_idx_ = 0;
    }

    void beginTuple() override {
        // Collect all tuples from child
        sorted_tuples_.clear();
        prev_->beginTuple();
        size_t idx = 0;
        while (!prev_->is_end()) {
            sorted_tuples_.push_back({prev_->Next(), idx});
            idx++;
            prev_->nextTuple();
        }
        tuple_num_ = sorted_tuples_.size();
        
        // Sort the tuples
        auto &prev_cols = prev_->cols();
        int key_offset = cols_.offset;
        ColType key_type = cols_.type;
        int key_len = cols_.len;
        
        std::sort(sorted_tuples_.begin(), sorted_tuples_.end(),
            [&](const auto &a, const auto &b) {
                const char *da = a.first->data + key_offset;
                const char *db = b.first->data + key_offset;
                int cmp;
                if (key_type == TYPE_INT) {
                    int ia = *(int *)da, ib = *(int *)db;
                    cmp = (ia < ib) ? -1 : ((ia > ib) ? 1 : 0);
                } else if (key_type == TYPE_FLOAT) {
                    float fa = *(float *)da, fb = *(float *)db;
                    cmp = (fa < fb) ? -1 : ((fa > fb) ? 1 : 0);
                } else {
                    cmp = memcmp(da, db, key_len);
                }
                return is_desc_ ? (cmp > 0) : (cmp < 0);
            });
        
        current_idx_ = 0;
    }

    void nextTuple() override {
        current_idx_++;
    }

    bool is_end() const override {
        return current_idx_ >= tuple_num_;
    }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        return std::move(sorted_tuples_[current_idx_].first);
    }

    size_t tupleLen() const override { return prev_->tupleLen(); }
    const std::vector<ColMeta> &cols() const override { return prev_->cols(); }
    ColMeta get_col_offset(const TabCol &target) override {
        return prev_->get_col_offset(target);
    }
    Rid &rid() override { return _abstract_rid; }
};
