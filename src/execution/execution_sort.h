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
#include <algorithm>

class SortExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> prev_;
    ColMeta sort_col_;
    bool is_desc_;
    std::vector<std::unique_ptr<RmRecord>> all_tuples_;
    size_t tuple_idx_;

   public:
    SortExecutor(std::unique_ptr<AbstractExecutor> prev, TabCol sel_cols, bool is_desc) {
        prev_ = std::move(prev);
                // Look up sort column from child executor's cols
        auto& pc = prev_->cols();
        for (auto& col : pc) {
            if (col.name == sel_cols.col_name && (sel_cols.tab_name.empty() || col.tab_name == sel_cols.tab_name)) {
                sort_col_ = col;
                break;
            }
        }
        is_desc_ = is_desc;
        tuple_idx_ = 0;
    }

    void beginTuple() override {
        prev_->beginTuple();
        while (!prev_->is_end()) {
            auto rec = prev_->Next();
            if (rec) {
                all_tuples_.push_back(std::move(rec));
            }
            prev_->nextTuple();
        }
        std::sort(all_tuples_.begin(), all_tuples_.end(),
            [this](const std::unique_ptr<RmRecord>& a, const std::unique_ptr<RmRecord>& b) {
                int cmp = ix_compare(a->data + sort_col_.offset, b->data + sort_col_.offset,
                                    sort_col_.type, sort_col_.len);
                return is_desc_ ? cmp > 0 : cmp < 0;
            });
        tuple_idx_ = 0;
    }

    void nextTuple() override {
        tuple_idx_++;
    }

    std::unique_ptr<RmRecord> Next() override {
        if (tuple_idx_ < all_tuples_.size()) {
            return std::make_unique<RmRecord>(*all_tuples_[tuple_idx_]);
        }
        return nullptr;
    }

    bool is_end() const override { return tuple_idx_ >= all_tuples_.size(); }

    Rid &rid() override { return _abstract_rid; }

    const std::vector<ColMeta>& cols() const override { return prev_->cols(); }

    size_t tupleLen() const override { return prev_->tupleLen(); }

    ColMeta get_col_offset(const TabCol& target) override {
        // Properly look up column in prev_ executor's cols
        auto& prev_cols = prev_->cols();
        for (auto& col : prev_cols) {
            if (col.name == target.col_name && (target.tab_name.empty() || col.tab_name == target.tab_name)) {
                return col;
            }
        }
        return ColMeta();
    }

    std::vector<AbstractExecutor*> get_children() override { return {prev_.get()}; }
};
