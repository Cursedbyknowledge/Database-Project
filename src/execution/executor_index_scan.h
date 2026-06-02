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

#include <climits>
#include <cmath>

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

    Rid rid_;
    std::unique_ptr<RecScan> scan_;

    SmManager *sm_manager_;
    IxIndexHandle *ih_;
    bool index_scan_usable_;

    const Condition* find_cond_for_col(const std::string& col_name) {
        for (auto& cond : conds_) {
            if (cond.lhs_col.col_name == col_name && cond.is_rhs_val) {
                return &cond;
            }
        }
        return nullptr;
    }

    struct ColBounds {
        const Condition* lower_cond = nullptr;
        const Condition* upper_cond = nullptr;
        const Condition* eq_cond = nullptr;
    };

    ColBounds find_col_bounds(const ColMeta& col) {
        ColBounds bounds;
        for (auto& cond : conds_) {
            if (cond.lhs_col.col_name != col.name || !cond.is_rhs_val) continue;
            if (cond.op == OP_EQ) {
                bounds.eq_cond = &cond;
            } else if (cond.op == OP_GT || cond.op == OP_GE) {
                if (bounds.lower_cond == nullptr) {
                    bounds.lower_cond = &cond;
                } else {
                    int cmp = ix_compare(cond.rhs_val.raw->data, bounds.lower_cond->rhs_val.raw->data,
                                         col.type, col.len);
                    if (cmp > 0) bounds.lower_cond = &cond;
                }
            } else if (cond.op == OP_LT || cond.op == OP_LE) {
                if (bounds.upper_cond == nullptr) {
                    bounds.upper_cond = &cond;
                } else {
                    int cmp = ix_compare(cond.rhs_val.raw->data, bounds.upper_cond->rhs_val.raw->data,
                                         col.type, col.len);
                    if (cmp < 0) bounds.upper_cond = &cond;
                }
            }
        }
        return bounds;
    }

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

   public:
    IndexScanExecutor(SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, std::vector<std::string> index_col_names,
                    Context *context) {
        sm_manager_ = sm_manager;
        context_ = context;
        tab_name_ = std::move(tab_name);
        tab_ = sm_manager_->db_.get_table(tab_name_);
        conds_ = std::move(conds);
        index_col_names_ = index_col_names;
        index_meta_ = *(tab_.get_index_meta_prefix(index_col_names_));
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        cols_ = tab_.cols;
        len_ = cols_.back().offset + cols_.back().len;
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

        ih_ = sm_manager_->get_ih(tab_name_, index_meta_.cols);

        index_scan_usable_ = false;
        auto first_cond = find_cond_for_col(index_meta_.cols[0].name);
        if (first_cond != nullptr) {
            index_scan_usable_ = true;
        }
    }

    void beginTuple() override {
        if (!index_scan_usable_) {
            scan_ = std::make_unique<RmScan>(fh_);
            rid_ = scan_->rid();
            while (!scan_->is_end()) {
                auto rec = fh_->get_record(rid_, context_);
                if (eval_conds(rec->data)) return;
                scan_->next();
                rid_ = scan_->rid();
            }
            return;
        }

        int col_tot_len = index_meta_.col_tot_len;
        char* low_key = new char[col_tot_len];
        char* high_key = new char[col_tot_len];

        auto fill_key_min = [&](char* buf, int start_offset) {
            int off = 0;
            for (auto& c : index_meta_.cols) {
                if (off >= start_offset) {
                    if (c.type == TYPE_INT) {
                        int min_val = INT_MIN;
                        memcpy(buf + off, &min_val, c.len);
                    } else if (c.type == TYPE_FLOAT) {
                        float min_val = -INFINITY;
                        memcpy(buf + off, &min_val, c.len);
                    } else {
                        memset(buf + off, 0, c.len);
                    }
                }
                off += c.len;
            }
        };

        auto fill_key_max = [&](char* buf, int start_offset) {
            int off = 0;
            for (auto& c : index_meta_.cols) {
                if (off >= start_offset) {
                    if (c.type == TYPE_INT) {
                        int max_val = INT_MAX;
                        memcpy(buf + off, &max_val, c.len);
                    } else if (c.type == TYPE_FLOAT) {
                        float max_val = INFINITY;
                        memcpy(buf + off, &max_val, c.len);
                    } else {
                        memset(buf + off, 0xFF, c.len);
                    }
                }
                off += c.len;
            }
        };

        fill_key_min(low_key, 0);
        fill_key_max(high_key, 0);

        Iid scan_start, scan_end;
        bool use_leaf_begin = true;
        bool use_leaf_end = true;
        bool start_upper = false;
        bool end_lower = false;

        for (size_t i = 0; i < index_meta_.cols.size(); i++) {
            auto& col = index_meta_.cols[i];

            if (i == 0) {
                auto bounds = find_col_bounds(col);
                if (bounds.eq_cond != nullptr) {
                    const char* val_data = bounds.eq_cond->rhs_val.raw->data;
                    memcpy(low_key, val_data, col.len);
                    memcpy(high_key, val_data, col.len);
                    use_leaf_begin = false;
                    use_leaf_end = false;

                    if (bounds.lower_cond != nullptr) {
                        const char* low_data = bounds.lower_cond->rhs_val.raw->data;
                        memcpy(low_key, low_data, col.len);
                        if (bounds.lower_cond->op == OP_GE) {
                            fill_key_min(low_key, col.len);
                        } else {
                            fill_key_max(low_key, col.len);
                        }
                        start_upper = (bounds.lower_cond->op == OP_GT);
                    }
                    if (bounds.upper_cond != nullptr) {
                        const char* up_data = bounds.upper_cond->rhs_val.raw->data;
                        memcpy(high_key, up_data, col.len);
                        if (bounds.upper_cond->op == OP_LE) {
                            fill_key_max(high_key, col.len);
                        } else {
                            fill_key_min(high_key, col.len);
                        }
                        end_lower = (bounds.upper_cond->op == OP_LT);
                    }
                    int offset = col.len;
                    for (size_t j = 1; j < index_meta_.cols.size(); j++) {
                        auto& next_col = index_meta_.cols[j];
                        auto next_cond = find_cond_for_col(next_col.name);
                        if (next_cond == nullptr) break;
                        const char* next_val = next_cond->rhs_val.raw->data;
                        if (next_cond->op == OP_EQ) {
                            memcpy(low_key + offset, next_val, next_col.len);
                            memcpy(high_key + offset, next_val, next_col.len);
                            offset += next_col.len;
                        } else if (next_cond->op == OP_GE || next_cond->op == OP_GT) {
                            memcpy(low_key + offset, next_val, next_col.len);
                            if (next_cond->op == OP_GE) {
                                fill_key_min(low_key, offset + next_col.len);
                            } else {
                                fill_key_max(low_key, offset + next_col.len);
                                start_upper = true;
                            }
                            break;
                        } else if (next_cond->op == OP_LE || next_cond->op == OP_LT) {
                            memcpy(high_key + offset, next_val, next_col.len);
                            if (next_cond->op == OP_LE) {
                                fill_key_max(high_key, offset + next_col.len);
                            } else {
                                fill_key_min(high_key, offset + next_col.len);
                            }
                            end_lower = (next_cond->op == OP_LT);
                            break;
                        }
                        offset += next_col.len;
                    }
                    break;
                }
                if (bounds.lower_cond != nullptr) {
                    const char* val_data = bounds.lower_cond->rhs_val.raw->data;
                    memcpy(low_key, val_data, col.len);
                    if (bounds.lower_cond->op == OP_GE) {
                        fill_key_min(low_key, col.len);
                    } else {
                        fill_key_max(low_key, col.len);
                    }
                    use_leaf_begin = false;
                    start_upper = (bounds.lower_cond->op == OP_GT);
                }
                if (bounds.upper_cond != nullptr) {
                    const char* val_data = bounds.upper_cond->rhs_val.raw->data;
                    memcpy(high_key, val_data, col.len);
                    if (bounds.upper_cond->op == OP_LE) {
                        fill_key_max(high_key, col.len);
                    } else {
                        fill_key_min(high_key, col.len);
                    }
                    use_leaf_end = false;
                    end_lower = (bounds.upper_cond->op == OP_LT);
                }
                break;
            }

            auto cond = find_cond_for_col(col.name);
            if (cond == nullptr) break;

            int offset = 0;
            for (size_t j = 0; j < i; j++) {
                offset += index_meta_.cols[j].len;
            }
            const char* val_data = cond->rhs_val.raw->data;

            if (cond->op == OP_EQ) {
                memcpy(low_key + offset, val_data, col.len);
                memcpy(high_key + offset, val_data, col.len);
            } else if (cond->op == OP_GE || cond->op == OP_GT) {
                memcpy(low_key + offset, val_data, col.len);
                if (cond->op == OP_GE) {
                    fill_key_min(low_key, offset + col.len);
                } else {
                    fill_key_max(low_key, offset + col.len);
                }
                break;
            } else if (cond->op == OP_LE || cond->op == OP_LT) {
                memcpy(high_key + offset, val_data, col.len);
                if (cond->op == OP_LE) {
                    fill_key_max(high_key, offset + col.len);
                } else {
                    fill_key_min(high_key, offset + col.len);
                }
                end_lower = (cond->op == OP_LT);
                break;
            }
        }

        if (use_leaf_begin) {
            scan_start = ih_->leaf_begin();
        } else if (start_upper) {
            scan_start = ih_->upper_bound(low_key);
        } else {
            scan_start = ih_->lower_bound(low_key);
        }

        if (use_leaf_end) {
            scan_end = ih_->leaf_end();
        } else if (end_lower) {
            scan_end = ih_->lower_bound(high_key);
        } else {
            scan_end = ih_->upper_bound(high_key);
        }

        delete[] low_key;
        delete[] high_key;

        scan_ = std::make_unique<IxScan>(ih_, scan_start, scan_end, sm_manager_->get_bpm());

        if (!scan_->is_end()) {
            rid_ = scan_->rid();
            while (!scan_->is_end()) {
                auto rec = fh_->get_record(rid_, context_);
                if (eval_conds(rec->data)) return;
                scan_->next();
                if (!scan_->is_end()) {
                    rid_ = scan_->rid();
                }
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
                if (!scan_->is_end()) {
                    rid_ = scan_->rid();
                }
            }
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