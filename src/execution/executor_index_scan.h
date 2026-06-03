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
    std::string tab_name_;                      // 表名称
    TabMeta tab_;                               // 表的元数据
    std::vector<Condition> conds_;              // 扫描条件
    RmFileHandle *fh_;                          // 表的数据文件句柄
    std::vector<ColMeta> cols_;                 // 需要读取的字段
    size_t len_;                                // 选取出来的一条记录的长度
    std::vector<Condition> fed_conds_;          // 扫描条件，和conds_字段相同

    std::vector<std::string> index_col_names_;  // index scan涉及到的索引包含的字段
    IndexMeta index_meta_;                      // index scan涉及到的索引元数据

    Rid rid_;
    std::unique_ptr<RecScan> scan_;

    SmManager *sm_manager_;

    // Compare values based on type
    int val_compare(const char *a, const char *b, ColType type, int len) {
        if (type == TYPE_INT) {
            int ia = *(int *)a, ib = *(int *)b;
            return (ia < ib) ? -1 : ((ia > ib) ? 1 : 0);
        } else if (type == TYPE_FLOAT) {
            float fa = *(float *)a, fb = *(float *)b;
            return (fa < fb) ? -1 : ((fa > fb) ? 1 : 0);
        } else {
            return memcmp(a, b, len);
        }
    }

    // Check if a record satisfies non-index conditions
    bool satisfy_conds(const RmRecord *rec) {
        if (fed_conds_.empty()) return true;
        for (auto &cond : fed_conds_) {
            auto col_iter = get_col(cols_, cond.lhs_col);
            int offset = col_iter->offset;
            char *lhs_data = rec->data + offset;
            
            char *rhs_data;
            if (!cond.is_rhs_val) {
                auto rhs_iter = get_col(cols_, cond.rhs_col);
                rhs_data = rec->data + rhs_iter->offset;
            } else {
                rhs_data = cond.rhs_val.raw->data;
            }
            
            int cmp = val_compare(lhs_data, rhs_data, col_iter->type, col_iter->len);
            switch (cond.op) {
                case OP_EQ: if (cmp != 0) return false; break;
                case OP_NE: if (cmp == 0) return false; break;
                case OP_LT: if (cmp >= 0) return false; break;
                case OP_GT: if (cmp <= 0) return false; break;
                case OP_LE: if (cmp > 0) return false; break;
                case OP_GE: if (cmp < 0) return false; break;
            }
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
        index_meta_ = *(tab_.get_index_meta(index_col_names_));
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
    }

    void beginTuple() override {
        // Build index key from conditions
        auto ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index_col_names_);
        IxIndexHandle *ih = sm_manager_->ihs_.at(ix_name).get();
        
        // Find the condition that applies to the index columns
        Condition *idx_cond = nullptr;
        for (auto &cond : fed_conds_) {
            if (cond.lhs_col.col_name == index_col_names_[0]) {
                idx_cond = &cond;
                break;
            }
        }
        
        if (idx_cond != nullptr && idx_cond->is_rhs_val) {
            // Build key and scan from the matching entry
            char key[index_meta_.col_tot_len];
            memset(key, 0, index_meta_.col_tot_len);
            memcpy(key, idx_cond->rhs_val.raw->data, index_meta_.col_tot_len);
            
            Iid lower = ih->lower_bound(key);
            Iid upper = ih->upper_bound(key);
            scan_ = std::make_unique<IxScan>(ih, lower, upper, sm_manager_->buffer_pool_manager_);
        } else {
            // Full index scan
            Iid lower = ih->leaf_begin();
            Iid upper = ih->leaf_end();
            scan_ = std::make_unique<IxScan>(ih, lower, upper, sm_manager_->buffer_pool_manager_);
        }
        
        // Advance to first matching record
        while (!scan_->is_end()) {
            rid_ = scan_->rid();
            auto rec = fh_->get_record(rid_, context_);
            if (satisfy_conds(rec.get())) return;
            scan_->next();
        }
    }

    void nextTuple() override {
        scan_->next();
        while (!scan_->is_end()) {
            rid_ = scan_->rid();
            auto rec = fh_->get_record(rid_, context_);
            if (satisfy_conds(rec.get())) return;
            scan_->next();
        }
    }

    bool is_end() const override {
        return scan_ == nullptr || scan_->is_end();
    }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        return fh_->get_record(rid_, context_);
    }

    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }
    ColMeta get_col_offset(const TabCol &target) override {
        auto pos = get_col(cols_, target);
        return *pos;
    }
    Rid &rid() override { return rid_; }
    // INLJ support: expose index handle and file handle
    RmFileHandle* get_fh() { return fh_; }
    IxIndexHandle* get_ih() { 
        auto ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index_col_names_);
        return sm_manager_->ihs_.at(ix_name).get();
    }
};
