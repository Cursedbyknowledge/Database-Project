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

#include <cstring>

#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class IndexScanExecutor : public AbstractExecutor {
   private:
    std::string tab_name_;
    TabMeta tab_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<ColMeta> cols_;
    size_t len_;
    std::vector<Condition> fed_conds_;

    std::vector<std::string> index_col_names_;
    IndexMeta index_meta_;
    IxIndexHandle *ih_;

    Rid rid_;
    std::unique_ptr<RecScan> scan_;

    SmManager *sm_manager_;

    // ----- 条件评估（与 SeqScanExecutor 一致）-----
    bool eval_cond(const Condition &cond, const char *rec_data) {
        const auto &tab_cols = sm_manager_->db_.get_table(tab_name_).cols;
        auto lhs_it = std::find_if(tab_cols.begin(), tab_cols.end(),
                                   [&](const ColMeta &col) { return col.name == cond.lhs_col.col_name; });
        const char *lhs_ptr = rec_data + lhs_it->offset;
        if (cond.is_rhs_val) {
            if (lhs_it->type == TYPE_INT) {
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
            } else if (lhs_it->type == TYPE_FLOAT) {
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
            } else if (lhs_it->type == TYPE_STRING) {
                std::string lhs_val(lhs_ptr, strnlen(lhs_ptr, lhs_it->len));
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
        }
        return true;
    }

    bool eval_conds(const char *rec_data) {
        for (auto &cond : fed_conds_) {
            if (!eval_cond(cond, rec_data)) return false;
        }
        return true;
    }

    // ----- 根据最左前缀条件构建 B+ 树扫描键 -----
    // low_key / high_key: 大小 = index_meta_.col_tot_len
    // 对每一列，收集所有匹配条件（等值、下界、上界）
    // 等值条件同时填入 low 和 high
    // 下界条件 (>, >=) 只填 low
    // 上界条件 (<, <=) 只填 high
    // needs_upper: 是否需要 upper_bound 截断
    void build_scan_keys(char *low_key, char *high_key, bool &needs_upper) {
        int col_tot_len = index_meta_.col_tot_len;
        memset(low_key, 0, col_tot_len);
        memset(high_key, 0xFF, col_tot_len);
        needs_upper = false;

        int key_offset = 0;
        for (size_t i = 0; i < index_col_names_.size(); i++) {
            // 对本列收集所有三种类型的条件
            bool has_eq = false;
            bool has_lower = false;
            bool has_upper = false;
            const char *eq_val = nullptr;
            const char *lower_val = nullptr;
            const char *upper_val = nullptr;

            for (auto &c : conds_) {
                if (!c.is_rhs_val || c.lhs_col.col_name != index_col_names_[i]) continue;
                if (c.op == OP_EQ) {
                    has_eq = true;
                    eq_val = c.rhs_val.raw->data;
                } else if (c.op == OP_GT || c.op == OP_GE) {
                    has_lower = true;
                    lower_val = c.rhs_val.raw->data;
                } else if (c.op == OP_LT || c.op == OP_LE) {
                    has_upper = true;
                    upper_val = c.rhs_val.raw->data;
                }
            }

            auto &col_meta = index_meta_.cols[i];

            if (has_eq) {
                // 等值：low 和 high 填相同值
                memcpy(low_key + key_offset, eq_val, col_meta.len);
                memcpy(high_key + key_offset, eq_val, col_meta.len);
                // 纯等值匹配时也需要 upper_bound 收束范围
                needs_upper = true;
                // 继续处理下一列
            } else if (has_lower || has_upper) {
                // 范围条件：这是最后一列匹配
                if (has_lower) {
                    memcpy(low_key + key_offset, lower_val, col_meta.len);
                }
                if (has_upper) {
                    memcpy(high_key + key_offset, upper_val, col_meta.len);
                    needs_upper = true;
                }
                break;  // 范围之后不再匹配后续列
            } else {
                // 该列没有任何条件，无法继续
                break;
            }
            key_offset += col_meta.len;
        }
    }

   public:
    IndexScanExecutor(SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds,
                      std::vector<std::string> index_col_names, Context *context) {
        sm_manager_ = sm_manager;
        context_ = context;
        tab_name_ = std::move(tab_name);
        tab_ = sm_manager_->db_.get_table(tab_name_);
        conds_ = std::move(conds);
        index_col_names_ = index_col_names;
        index_meta_ = *(tab_.get_index_meta(index_col_names_));
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        cols_ = tab_.cols;
        len_ = cols_.back().offset + cols_.back().len;

        // 获取 B+ 树索引句柄
        std::string ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index_meta_.cols);
        ih_ = sm_manager_->ihs_.at(ix_name).get();

        // 规范化条件：确保 lhs_col 指向本表
        std::map<CompOp, CompOp> swap_op = {
            {OP_EQ, OP_EQ}, {OP_NE, OP_NE}, {OP_LT, OP_GT}, {OP_GT, OP_LT}, {OP_LE, OP_GE}, {OP_GE, OP_LE},
        };
        for (auto &cond : conds_) {
            if (cond.lhs_col.tab_name != tab_name_) {
                assert(!cond.is_rhs_val && cond.rhs_col.tab_name == tab_name_);
                std::swap(cond.lhs_col, cond.rhs_col);
                cond.op = swap_op.at(cond.op);
            }
        }
        fed_conds_ = conds_;  // 全部条件作为后过滤
    }

    void beginTuple() override {
        int col_tot_len = index_meta_.col_tot_len;
        char *low_key = new char[col_tot_len];
        char *high_key = new char[col_tot_len];
        bool needs_upper = false;
        build_scan_keys(low_key, high_key, needs_upper);

        // 确定 B+ 树扫描范围:
        //   needs_upper == true  → upper_bound(high_key) 收束（等值或上界范围）
        //   needs_upper == false → leaf_end() 不限上界（纯下界范围）
        Iid lower = ih_->lower_bound(low_key);
        Iid upper = needs_upper ? ih_->upper_bound(high_key) : ih_->leaf_end();

        delete[] low_key;
        delete[] high_key;

        // 使用 IxScan 遍历 B+ 树叶子节点
        scan_ = std::make_unique<IxScan>(ih_, lower, upper, sm_manager_->get_bpm());

        // 找到第一条满足所有条件的记录（后过滤）
        if (!scan_->is_end()) {
            rid_ = scan_->rid();
            while (!scan_->is_end()) {
                auto rec = fh_->get_record(rid_, context_);
                if (eval_conds(rec->data)) return;
                scan_->next();
                if (!scan_->is_end()) rid_ = scan_->rid();
            }
        }
    }

    void nextTuple() override {
        scan_->next();
        if (!scan_->is_end()) {
            rid_ = scan_->rid();
            while (!scan_->is_end()) {
                auto rec = fh_->get_record(rid_, context_);
                if (eval_conds(rec->data)) return;
                scan_->next();
                if (!scan_->is_end()) rid_ = scan_->rid();
            }
        }
    }

    bool is_end() const override { return scan_->is_end(); }

    std::unique_ptr<RmRecord> Next() override { return fh_->get_record(rid_, context_); }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    Rid &rid() override { return rid_; }
};
