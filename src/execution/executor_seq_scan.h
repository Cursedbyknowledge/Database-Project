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

class SeqScanExecutor : public AbstractExecutor {
   private:
    std::string tab_name_;              // 表的名称
    std::vector<Condition> conds_;      // scan的条件
    RmFileHandle *fh_;                  // 表的数据文件句柄
    std::vector<ColMeta> cols_;         // scan后生成的记录的字段
    size_t len_;                        // scan后生成的每条记录的长度
    std::vector<Condition> fed_conds_;  // 同conds_，两个字段相同

    Rid rid_;
    std::unique_ptr<RecScan> scan_;     // table_iterator

    SmManager *sm_manager_;

    // Helper: check if a record satisfies all conditions
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
            
            int cmp_result;
            if (col_iter->type == TYPE_INT) {
                int lhs = *(int *)lhs_data;
                int rhs = *(int *)rhs_data;
                cmp_result = (lhs < rhs) ? -1 : ((lhs > rhs) ? 1 : 0);
            } else if (col_iter->type == TYPE_FLOAT) {
                float lhs = *(float *)lhs_data;
                float rhs = *(float *)rhs_data;
                cmp_result = (lhs < rhs) ? -1 : ((lhs > rhs) ? 1 : 0);
            } else {
                cmp_result = memcmp(lhs_data, rhs_data, col_iter->len);
            }
            
            switch (cond.op) {
                case OP_EQ: if (cmp_result != 0) return false; break;
                case OP_NE: if (cmp_result == 0) return false; break;
                case OP_LT: if (cmp_result >= 0) return false; break;
                case OP_GT: if (cmp_result <= 0) return false; break;
                case OP_LE: if (cmp_result > 0) return false; break;
                case OP_GE: if (cmp_result < 0) return false; break;
            }
        }
        return true;
    }

   public:
    SeqScanExecutor(SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, Context *context) {
        sm_manager_ = sm_manager;
        tab_name_ = std::move(tab_name);
        conds_ = std::move(conds);
        TabMeta &tab = sm_manager_->db_.get_table(tab_name_);
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        cols_ = tab.cols;
        len_ = cols_.empty() ? 0 : (cols_.back().offset + cols_.back().len);

        context_ = context;

        fed_conds_ = conds_;
    }

    void beginTuple() override {
        scan_ = std::make_unique<RmScan>(fh_);
        // Advance to first matching record
        while (!scan_->is_end()) {
            auto rec = fh_->get_record(scan_->rid(), context_);
            if (satisfy_conds(rec.get())) {
                rid_ = scan_->rid();
                return;
            }
            scan_->next();
        }
    }

    void nextTuple() override {
        scan_->next();
        while (!scan_->is_end()) {
            auto rec = fh_->get_record(scan_->rid(), context_);
            if (satisfy_conds(rec.get())) {
                rid_ = scan_->rid();
                return;
            }
            scan_->next();
        }
    }

    bool is_end() const override {
        return scan_ == nullptr || scan_->is_end();
    }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        auto rec = fh_->get_record(rid_, context_);
        return rec;
    }

    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }
    ColMeta get_col_offset(const TabCol &target) override {
        auto pos = get_col(cols_, target);
        return *pos;
    }
    Rid &rid() override { return rid_; }
};
