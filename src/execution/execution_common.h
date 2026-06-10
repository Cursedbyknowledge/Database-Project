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

// Evaluate a join condition across two records
inline bool eval_cond_join(const char* left_data, const char* right_data, const Condition& cond,
                           const std::vector<ColMeta>& left_cols, const std::vector<ColMeta>& right_cols) {
    auto lhs_col = std::find_if(left_cols.begin(), left_cols.end(), [&](const ColMeta& c) {
        return c.tab_name == cond.lhs_col.tab_name && c.name == cond.lhs_col.col_name;
    });
    auto rhs_col = std::find_if(right_cols.begin(), right_cols.end(), [&](const ColMeta& c) {
        return c.tab_name == cond.rhs_col.tab_name && c.name == cond.rhs_col.col_name;
    });
    if (lhs_col == left_cols.end() || rhs_col == right_cols.end()) return true;
    const char* lv = left_data + lhs_col->offset;
    const char* rv = right_data + rhs_col->offset;
    int cmp = ix_compare(lv, rv, lhs_col->type, lhs_col->len);
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
