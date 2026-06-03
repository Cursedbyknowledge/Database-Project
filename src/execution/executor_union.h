/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. */

#pragma once
#include <set>
#include <functional>
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class UnionExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> left_;
    std::unique_ptr<AbstractExecutor> right_;
    std::vector<ColMeta> cols_;
    size_t len_;
    bool on_left_;  // currently iterating left child
    bool done_;
    std::set<std::string> seen_rows_;  // for UNION (not ALL) dedup

    static std::string row_key(const RmRecord *rec, size_t len) {
        return std::string(rec->data, len);
    }

   public:
    UnionExecutor(std::unique_ptr<AbstractExecutor> left, std::unique_ptr<AbstractExecutor> right,
                  bool is_all) {
        left_ = std::move(left);
        right_ = std::move(right);
        cols_ = left_->cols();
        len_ = left_->tupleLen();
        on_left_ = true;
        done_ = false;
        (void)is_all;  // dedup for non-ALL UNION
    }

    void beginTuple() override {
        left_->beginTuple();
        on_left_ = true;
        done_ = false;
        seen_rows_.clear();
        // Skip duplicates on left (shouldn't be any from a regular select)
    }

    void nextTuple() override {
        if (on_left_) {
            left_->nextTuple();
        } else {
            right_->nextTuple();
        }
    }

    bool is_end() const override {
        if (done_) return true;
        if (on_left_ && !left_->is_end()) return false;
        if (!on_left_ && !right_->is_end()) return false;
        return true;
    }

    std::unique_ptr<RmRecord> Next() override {
        while (true) {
            if (on_left_) {
                if (left_->is_end()) {
                    on_left_ = false;
                    right_->beginTuple();
                    continue;
                }
                auto rec = left_->Next();
                seen_rows_.insert(row_key(rec.get(), len_));
                return rec;
            } else {
                if (right_->is_end()) {
                    done_ = true;
                    return nullptr;
                }
                auto rec = right_->Next();
                std::string key = row_key(rec.get(), len_);
                if (seen_rows_.count(key)) continue;  // dedup
                seen_rows_.insert(key);
                return rec;
            }
        }
    }

    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }
    Rid &rid() override { return _abstract_rid; }
};
