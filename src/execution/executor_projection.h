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

class ProjectionExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> prev_;
    std::vector<ColMeta> cols_;
    size_t len_;
    std::vector<size_t> sel_idxs_;

   public:
    ProjectionExecutor(std::unique_ptr<AbstractExecutor> prev, const std::vector<TabCol> &sel_cols) {
        prev_ = std::move(prev);
        size_t curr_offset = 0;
        auto &prev_cols = prev_->cols();
        // SELECT *：全量投影（analyzer已展开*为实际列名，此处防御性保留）
        if (sel_cols.empty() || (sel_cols.size() == 1 && sel_cols[0].col_name == "*")) {
            for (size_t i = 0; i < prev_cols.size(); ++i) {
                ColMeta col_meta = prev_cols[i];
                col_meta.offset = curr_offset;
                curr_offset += col_meta.len;
                cols_.push_back(col_meta);
                sel_idxs_.push_back(i);
            }
        } else {
            for (auto &sel_col : sel_cols) {
                auto pos = get_col(prev_cols, sel_col);
                sel_idxs_.push_back(pos - prev_cols.begin());
                auto col = *pos;
                col.offset = curr_offset;
                curr_offset += col.len;
                cols_.push_back(col);
            }
        }
        len_ = curr_offset;
    }

    void beginTuple() override { prev_->beginTuple(); }

    void nextTuple() override { prev_->nextTuple(); }

    std::unique_ptr<RmRecord> Next() override {
        auto prev_rec = prev_->Next();
        if (!prev_rec) return nullptr;
        runtime_rows_++;  // Project rows: 投影输出行数
        runtime_output_++;
        auto proj_rec = std::make_unique<RmRecord>(len_);
        for (size_t i = 0; i < sel_idxs_.size(); i++) {
            auto& col = cols_[i];
            auto& prev_col = prev_->cols()[sel_idxs_[i]];
            memcpy(proj_rec->data + col.offset, prev_rec->data + prev_col.offset, col.len);
        }
        return proj_rec;
    }

    bool is_end() const override { return prev_->is_end(); }

    const std::vector<ColMeta> &cols() const override { return cols_; }
    size_t tupleLen() const override { return len_; }

    Rid &rid() override { return _abstract_rid; }
    std::vector<AbstractExecutor*> get_children() override { return {prev_.get()}; }

    // INLJ: 透传动态join key到子节点
    void set_dynamic_join_key(const std::string& col_name, const char* key_data, int key_len, ColType key_type) override {
        prev_->set_dynamic_join_key(col_name, key_data, key_len, key_type);
    }
};
