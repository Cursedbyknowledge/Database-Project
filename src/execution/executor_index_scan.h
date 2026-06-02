/* Copyright (c) 2023 Renmin University of China RMDB is licensed under Mulan PSL v2. */
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
    std::vector<Rid> idx_rids_;  // 等值查询时预收集的 Rid 列表
    size_t idx_pos_;             // 当前处理的 Rid 位置
    bool use_index_eq_;          // 是否使用 get_value 等值加速

    SmManager *sm_manager_;

    bool eval_cond(const Condition &cond, const char *rec_data) {
        const auto &tab_cols = sm_manager_->db_.get_table(tab_name_).cols;
        auto lhs_it = std::find_if(tab_cols.begin(), tab_cols.end(),
                                   [&](const ColMeta &c) { return c.name == cond.lhs_col.col_name; });
        const char *lp = rec_data + lhs_it->offset;
        if (cond.is_rhs_val) {
            if (lhs_it->type == TYPE_INT) {
                int lv = *(int *)lp, rv = cond.rhs_val.int_val;
                switch (cond.op) {
                    case OP_EQ: return lv == rv; case OP_NE: return lv != rv;
                    case OP_LT: return lv < rv;  case OP_GT: return lv > rv;
                    case OP_LE: return lv <= rv; case OP_GE: return lv >= rv;
                }
            } else if (lhs_it->type == TYPE_FLOAT) {
                float lv = *(float *)lp, rv = cond.rhs_val.float_val;
                switch (cond.op) {
                    case OP_EQ: return lv == rv; case OP_NE: return lv != rv;
                    case OP_LT: return lv < rv;  case OP_GT: return lv > rv;
                    case OP_LE: return lv <= rv; case OP_GE: return lv >= rv;
                }
            } else if (lhs_it->type == TYPE_STRING) {
                std::string lv(lp, strnlen(lp, lhs_it->len)), rv = cond.rhs_val.str_val;
                switch (cond.op) {
                    case OP_EQ: return lv == rv; case OP_NE: return lv != rv;
                    case OP_LT: return lv < rv;  case OP_GT: return lv > rv;
                    case OP_LE: return lv <= rv; case OP_GE: return lv >= rv;
                }
            }
        }
        return true;
    }

    bool eval_conds(const char *rec_data) {
        for (auto &cond : fed_conds_) if (!eval_cond(cond, rec_data)) return false;
        return true;
    }

    bool all_index_conds_eq() {
        for (size_t i = 0; i < index_col_names_.size(); i++) {
            bool found = false;
            for (auto &c : conds_)
                if (c.is_rhs_val && c.lhs_col.col_name == index_col_names_[i] && c.op == OP_EQ)
                    found = true;
            if (!found) return false;
        }
        return !index_col_names_.empty();
    }

    void build_eq_key(char *kbuf) {
        int off = 0;
        for (size_t i = 0; i < index_col_names_.size(); i++) {
            for (auto &c : conds_) {
                if (c.is_rhs_val && c.lhs_col.col_name == index_col_names_[i] && c.op == OP_EQ) {
                    memcpy(kbuf + off, c.rhs_val.raw->data, index_meta_.cols[i].len);
                    off += index_meta_.cols[i].len;
                    break;
                }
            }
        }
    }

   public:
    IndexScanExecutor(SmManager *sm, std::string tn, std::vector<Condition> cd,
                      std::vector<std::string> icn, Context *ctx) {
        sm_manager_ = sm; context_ = ctx;
        tab_name_ = std::move(tn); conds_ = std::move(cd); index_col_names_ = icn;
        tab_ = sm->db_.get_table(tab_name_);
        index_meta_ = *(tab_.get_index_meta(index_col_names_));
        fh_ = sm->fhs_.at(tab_name_).get();
        cols_ = tab_.cols; len_ = cols_.back().offset + cols_.back().len;
        ih_ = sm->get_ih(tab_name_, index_meta_.cols);
        use_index_eq_ = ih_ && all_index_conds_eq();
        idx_pos_ = 0;
        std::map<CompOp, CompOp> so = {{OP_EQ,OP_EQ},{OP_NE,OP_NE},{OP_LT,OP_GT},{OP_GT,OP_LT},{OP_LE,OP_GE},{OP_GE,OP_LE}};
        for (auto &c : conds_) {
            if (c.lhs_col.tab_name != tab_name_) {
                assert(!c.is_rhs_val && c.rhs_col.tab_name == tab_name_);
                std::swap(c.lhs_col, c.rhs_col); c.op = so.at(c.op);
            }
        }
        fed_conds_ = conds_;
    }

    void beginTuple() override {
        if (use_index_eq_) {
            // 等值索引加速：用 get_value 收集所有匹配 Rid
            char *k = new char[index_meta_.col_tot_len];
            build_eq_key(k);
            try { ih_->get_value(k, &idx_rids_, context_->txn_); }
            catch (...) { delete[] k; use_index_eq_ = false; }
            if (!use_index_eq_) {
                delete[] k;
                scan_ = std::make_unique<RmScan>(fh_);
                rid_ = scan_->rid();
                while (!scan_->is_end()) { auto rec=fh_->get_record(rid_,context_); if(eval_conds(rec->data))return; scan_->next();rid_=scan_->rid(); }
                return;
            }
            idx_pos_ = 0;
            // 后过滤找到第一条匹配记录
            scan_.reset();
            while (idx_pos_ < idx_rids_.size()) {
                rid_ = idx_rids_[idx_pos_++];
                auto rec = fh_->get_record(rid_, context_);
                if (eval_conds(rec->data)) return;
            }
        } else {
            // 回退：全表扫描
            scan_ = std::make_unique<RmScan>(fh_);
            rid_ = scan_->rid();
            while (!scan_->is_end()) {
                auto rec = fh_->get_record(rid_, context_);
                if (eval_conds(rec->data)) return;
                scan_->next(); rid_ = scan_->rid();
            }
        }
    }

    void nextTuple() override {
        if (use_index_eq_) {
            while (idx_pos_ < idx_rids_.size()) {
                rid_ = idx_rids_[idx_pos_++];
                auto rec = fh_->get_record(rid_, context_);
                if (eval_conds(rec->data)) return;
            }
        } else {
            scan_->next(); rid_ = scan_->rid();
            while (!scan_->is_end()) {
                auto rec = fh_->get_record(rid_, context_);
                if (eval_conds(rec->data)) return;
                scan_->next(); rid_ = scan_->rid();
            }
        }
    }

    bool is_end() const override {
        if (use_index_eq_) return idx_pos_ >= idx_rids_.size();
        return scan_ ? scan_->is_end() : true;
    }
    std::unique_ptr<RmRecord> Next() override { return fh_->get_record(rid_, context_); }
    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }
    Rid &rid() override { return rid_; }
};
