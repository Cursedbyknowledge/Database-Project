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
        
        // SELECT * 全投影
        if (sel_cols.empty() || (sel_cols.size() == 1 && sel_cols[0].col_name == "*")) {
            for (size_t i = 0; i < prev_cols.size(); ++i) {
                ColMeta col_meta = prev_cols[i];
                col_meta.offset = curr_offset;
                curr_offset += col_meta.len;
                cols_.push_back(col_meta);
                sel_idxs_.push_back(i);
            }
        } else {
            // 指定列投影，重排偏移量与名称
            for (auto &sel_col : sel_cols) {
                bool found = false;
                for (size_t i = 0; i < prev_cols.size(); ++i) {
                    if (prev_cols[i].name == sel_col.col_name) {
                        // 带有前缀的精准表名匹配
                        if (!sel_col.tab_name.empty() && !prev_cols[i].tab_name.empty() && sel_col.tab_name != prev_cols[i].tab_name) {
                            continue;
                        }
                        ColMeta col_meta = prev_cols[i];
                        col_meta.name = sel_col.col_name; // 将实际的别名/选取名写入表头
                        col_meta.offset = curr_offset;
                        curr_offset += col_meta.len;
                        cols_.push_back(col_meta);
                        sel_idxs_.push_back(i);
                        found = true;
                        break;
                    }
                }
                // (忽略未找到的情况，交由外部报错或容错处理)
            }
            // 异常兜底，防止意外崩溃
            if (cols_.size() != sel_cols.size()) {
                cols_ = prev_cols;
                sel_idxs_.clear();
                curr_offset = 0;
                for (size_t i = 0; i < prev_cols.size(); ++i) {
                    cols_[i].offset = curr_offset;
                    curr_offset += cols_[i].len;
                    sel_idxs_.push_back(i);
                }
            }
        }
        len_ = curr_offset;
    }

    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }

    void beginTuple() override { prev_->beginTuple(); }
    void nextTuple() override { prev_->nextTuple(); }
    bool is_end() const override { return prev_->is_end(); }
    Rid &rid() override { return _abstract_rid; }
    std::vector<AbstractExecutor*> get_children() override { return {prev_.get()}; }

    // 【核心大修复：重装内存布局】
    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        auto prev_rec = prev_->Next();
        if (prev_rec == nullptr) return nullptr;
        
        runtime_rows_++;
        runtime_output_++;

        auto new_rec = std::make_unique<RmRecord>(len_);
        // 根据映射关系将底层游标读出来的数据，一一拷贝到投影后的正确位置上！
        for (size_t i = 0; i < cols_.size(); i++) {
            auto &new_col = cols_[i];
            auto &prev_col = prev_->cols()[sel_idxs_[i]];
            memcpy(new_rec->data + new_col.offset, prev_rec->data + prev_col.offset, new_col.len);
        }
        return new_rec;
    }
};
