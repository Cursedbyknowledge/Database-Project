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

#include <vector>
#include <optional>


#include "transaction/transaction.h"
#include "transaction/transaction_manager.h"
#include "common/common.h"

auto ReconstructTuple(const TabMeta *schema, const RmRecord &base_tuple, const TupleMeta &base_meta,
                      const std::vector<UndoLog> &undo_logs) -> std::optional<RmRecord>;


auto IsWriteWriteConflict(timestamp_t tuple_ts, Transaction *txn) -> bool;

// Evaluate a condition against a single record
inline bool eval_cond(const char* rec_data, const Condition& cond, const std::vector<ColMeta>& cols) {
    auto lhs_col = std::find_if(cols.begin(), cols.end(), [&](const ColMeta& c) {
        return c.tab_name == cond.lhs_col.tab_name && c.name == cond.lhs_col.col_name;
    });
    if (lhs_col == cols.end()) return true;
    const char* lhs_val = rec_data + lhs_col->offset;
    const char* rhs_val = cond.rhs_val.raw->data;
    int cmp = ix_compare(lhs_val, rhs_val, lhs_col->type, lhs_col->len);
    switch (cond.op) {
        case OP_EQ: return cmp == 0;
        case OP_NE: return cmp != 0;
        case OP_LT: return cmp < 0;
        case OP_GT: return cmp > 0;
        case OP_LE: return cmp <= 0;
        case OP_GE: return cmp >= 0;
        default: return true;
    }
}

// Evaluate a join condition across two records.
// 多表NLJ中外层Join的fed_conds_可能包含两边列都在同侧数据的条件，
// 因此需要先在左右两侧自由查找lhs_col和rhs_col，而非硬编码lhs=左/rhs=右。
inline bool eval_cond_join(const char* left_data, const char* right_data, const Condition& cond,
                           const std::vector<ColMeta>& left_cols, const std::vector<ColMeta>& right_cols) {
    // 辅助：在指定列集合中查找目标列
    auto find_in = [](const std::vector<ColMeta>& cols, const TabCol& target) -> const ColMeta* {
        auto it = std::find_if(cols.begin(), cols.end(), [&](const ColMeta& c) {
            return c.tab_name == target.tab_name && c.name == target.col_name;
        });
        return (it != cols.end()) ? &(*it) : nullptr;
    };
    // 查找 lhs_col：先在左侧找，再到右侧找
    const ColMeta* lhs_meta = find_in(left_cols, cond.lhs_col);
    const char* lhs_val_ptr;
    if (lhs_meta) {
        lhs_val_ptr = left_data + lhs_meta->offset;
    } else {
        lhs_meta = find_in(right_cols, cond.lhs_col);
        if (!lhs_meta) return true;   // 列不存在，跳过条件
        lhs_val_ptr = right_data + lhs_meta->offset;
    }
    // 查找 rhs_col：先在右侧找，再到左侧找（与上面相反，优先匹配对侧）
    const ColMeta* rhs_meta = find_in(right_cols, cond.rhs_col);
    const char* rhs_val_ptr;
    if (rhs_meta) {
        rhs_val_ptr = right_data + rhs_meta->offset;
    } else {
        rhs_meta = find_in(left_cols, cond.rhs_col);
        if (!rhs_meta) return true;
        rhs_val_ptr = left_data + rhs_meta->offset;
    }
    int cmp = ix_compare(lhs_val_ptr, rhs_val_ptr, lhs_meta->type, lhs_meta->len);
    switch (cond.op) {
        case OP_EQ: return cmp == 0;
        case OP_NE: return cmp != 0;
        case OP_LT: return cmp < 0;
        case OP_GT: return cmp > 0;
        case OP_LE: return cmp <= 0;
        case OP_GE: return cmp >= 0;
        default: return true;
    }
}
