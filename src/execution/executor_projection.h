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
        
        // 应对 SELECT * 操作：全量投影
        if (sel_cols.size() == 1 && sel_cols[0].col_name == "*") {
            for (size_t i = 0; i < prev_cols.size(); ++i) {
                ColMeta col_meta = prev_cols[i];
                col_meta.offset = curr_offset;
                curr_offset += col_meta.len;
                cols_.push_back(col_meta);
                sel_idxs_.push_back(i);
            }
        } else {
            // 应对指定列的投影重排
            for (auto &sel_col : sel_cols) {
                ColMeta col_meta;
                bool found = false;
                for (size_t i = 0; i < prev_cols.size(); ++i) {
                    if (prev_cols[i].name == sel_col.col_name) {
                        // 【核心修复】：如果有表名，则必须严格匹配（防多表Join同名列冲突）
                        if (!sel_col.tab_name.empty() && !prev_cols[i].tab_name.empty() && sel_col.tab_name != prev_cols[i].tab_name) {
                            continue;
                        }
                        col_meta = prev_cols[i];
                        sel_idxs_.push_back(i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    throw RMDBError("Column not found in projection");
                }
                col_meta.offset = curr_offset;
                curr_offset += col_meta.len;
                cols_.push_back(col_meta);
            }
        }
        len_ = curr_offset;
    }

    void beginTuple() override { prev_->beginTuple(); }

    void nextTuple() override { prev_->nextTuple(); }

    bool is_end() const override { return prev_->is_end(); }

    std::unique_ptr<RmRecord> Next() override {
        auto prev_rec = prev_->Next();
        if (prev_rec == nullptr) return nullptr;
        
        runtime_rows_++;
        runtime_output_++;

        auto new_rec = std::make_unique<RmRecord>(len_);
        // 【核心大修复：重装内存布局！】
        // 必须根据原算子的 offset 和新算子的 offset，重新将数据拷贝对齐！
        for (size_t i = 0; i < cols_.size(); i++) {
            auto &new_col = cols_[i];
            auto &prev_col = prev_->cols()[sel_idxs_[i]];
            memcpy(new_rec->data + new_col.offset, prev_rec->data + prev_col.offset, new_col.len);
        }
        return new_rec;
    }

    Rid &rid() override { return _abstract_rid; }
    std::vector<AbstractExecutor*> get_children() override { return {prev_.get()}; }

    const std::vector<ColMeta> &cols() const override { return cols_; }
    size_t tupleLen() const override { return len_; }
};
