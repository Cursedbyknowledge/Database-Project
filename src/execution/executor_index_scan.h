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

    const Condition* find_cond_for_col(const std::string& col_name) {
        for (auto &cond : conds_) {
            if (cond.is_rhs_val && cond.lhs_col.col_name == col_name) return &cond;
        }
        return nullptr;
    }

    void build_scan_key(char* key_buf, bool is_low) {
        int offset = 0;
        for (auto &idx_col : index_meta_.cols) {
            const Condition* cond = find_cond_for_col(idx_col.name);
            if (cond != nullptr) {
                Value val = cond->rhs_val;
                if (val.type != idx_col.type) {
                    if (idx_col.type == TYPE_FLOAT && val.type == TYPE_INT) {
                        val.set_float((float)val.int_val);
                    } else if (idx_col.type == TYPE_INT && val.type == TYPE_FLOAT) {
                        val.set_int((int)val.float_val);
                    }
                }
                val.init_raw(idx_col.len);
                memcpy(key_buf + offset, val.raw->data, idx_col.len);

                if (is_low && cond->op == OP_GT) {
                    if (idx_col.type == TYPE_INT) {
                        int v = *(int*)(key_buf + offset);
                        v++;
                        memcpy(key_buf + offset, &v, sizeof(int));
                    }
                }

                if (!is_low && (cond->op == OP_LT)) {
                    if (idx_col.type == TYPE_INT) {
                        int v = *(int*)(key_buf + offset);
                        v--;
                        memcpy(key_buf + offset, &v, sizeof(int));
                    }
                    offset += idx_col.len;
                    memset(key_buf + offset, 0xFF, index_meta_.col_tot_len - offset);
                    return;
                }
            } else {
                if (is_low) {
                    memset(key_buf + offset, 0, index_meta_.col_tot_len - offset);
                } else {
                    memset(key_buf + offset, 0xFF, index_meta_.col_tot_len - offset);
                }
                return;
            }
            offset += idx_col.len;
        }
    }

   public:
    IndexScanExecutor(SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, std::vector<std::string> index_col_names,
                    Context *context) {
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

        std::string ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index_col_names_);
        ih_ = sm_manager_->ihs_.at(ix_name).get();

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
        fed_conds_ = conds_;
    }

    void beginTuple() override {
        std::vector<char> low_key(index_meta_.col_tot_len, 0);
        std::vector<char> high_key(index_meta_.col_tot_len, static_cast<char>(0xFF));

        build_scan_key(low_key.data(), true);
        build_scan_key(high_key.data(), false);

        Iid lower = ih_->lower_bound(low_key.data());
        Iid upper = ih_->upper_bound(high_key.data());

        scan_ = std::make_unique<IxScan>(ih_, lower, upper, sm_manager_->get_bpm());
        if (scan_->is_end()) return;

        rid_ = scan_->rid();
        while (!scan_->is_end()) {
            auto rec = fh_->get_record(rid_, context_);
            if (eval_conds(rec->data)) return;
            scan_->next();
            if (scan_->is_end()) return;
            rid_ = scan_->rid();
        }
    }

    void nextTuple() override {
        scan_->next();
        if (scan_->is_end()) return;
        rid_ = scan_->rid();
        while (!scan_->is_end()) {
            auto rec = fh_->get_record(rid_, context_);
            if (eval_conds(rec->data)) return;
            scan_->next();
            if (scan_->is_end()) return;
            rid_ = scan_->rid();
        }
    }

    bool is_end() const override {
        return scan_->is_end();
    }

    std::unique_ptr<RmRecord> Next() override {
        return fh_->get_record(rid_, context_);
    }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    Rid &rid() override { return rid_; }
};