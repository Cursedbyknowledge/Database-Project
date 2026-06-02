/* Copyright (c) 2023 Renmin University of China. RMDB is licensed under Mulan PSL v2. */
#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class IndexScanExecutor : public AbstractExecutor {
   private:
    std::string tab_name_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<ColMeta> cols_;
    size_t len_;
    std::vector<Condition> fed_conds_;
    Rid rid_;
    std::unique_ptr<RecScan> scan_;
    SmManager *sm_;

    bool eval_cond(const Condition &cond, const char *rec_data) {
        const auto &tc = sm_->db_.get_table(tab_name_).cols;
        auto it = std::find_if(tc.begin(), tc.end(), [&](const ColMeta &c){return c.name==cond.lhs_col.col_name;});
        const char *lp = rec_data + it->offset;
        if (!cond.is_rhs_val) return true;
        if (it->type == TYPE_INT) {
            int lv = *(int *)lp, rv = cond.rhs_val.int_val;
            switch (cond.op) { case OP_EQ:return lv==rv; case OP_NE:return lv!=rv; case OP_LT:return lv<rv; case OP_GT:return lv>rv; case OP_LE:return lv<=rv; case OP_GE:return lv>=rv; }
        } else if (it->type == TYPE_FLOAT) {
            float lv = *(float *)lp, rv = cond.rhs_val.float_val;
            switch (cond.op) { case OP_EQ:return lv==rv; case OP_NE:return lv!=rv; case OP_LT:return lv<rv; case OP_GT:return lv>rv; case OP_LE:return lv<=rv; case OP_GE:return lv>=rv; }
        } else {
            std::string lv(lp, strnlen(lp, it->len)), rv = cond.rhs_val.str_val;
            switch (cond.op) { case OP_EQ:return lv==rv; case OP_NE:return lv!=rv; case OP_LT:return lv<rv; case OP_GT:return lv>rv; case OP_LE:return lv<=rv; case OP_GE:return lv>=rv; }
        }
        return true;
    }
    bool eval_conds(const char *r) { for(auto&c:fed_conds_)if(!eval_cond(c,r))return false; return true; }

   public:
    IndexScanExecutor(SmManager *s, std::string tn, std::vector<Condition> cd,
                      std::vector<std::string> icn, Context *ctx) {
        sm_=s; context_=ctx; tab_name_=std::move(tn); conds_=std::move(cd);
        auto&tab=s->db_.get_table(tab_name_); cols_=tab.cols;
        len_=cols_.back().offset+cols_.back().len; fh_=s->fhs_.at(tab_name_).get();
        std::map<CompOp,CompOp> so={{OP_EQ,OP_EQ},{OP_NE,OP_NE},{OP_LT,OP_GT},{OP_GT,OP_LT},{OP_LE,OP_GE},{OP_GE,OP_LE}};
        for(auto&c:conds_){if(c.lhs_col.tab_name!=tab_name_){assert(!c.is_rhs_val&&c.rhs_col.tab_name==tab_name_);std::swap(c.lhs_col,c.rhs_col);c.op=so.at(c.op);}}
        fed_conds_=conds_;
    }

    void beginTuple() override {
        scan_=std::make_unique<RmScan>(fh_);
        rid_=scan_->rid();
        while(!scan_->is_end()){auto rec=fh_->get_record(rid_,context_);if(eval_conds(rec->data))return;scan_->next();rid_=scan_->rid();}
    }
    void nextTuple() override {scan_->next();rid_=scan_->rid();while(!scan_->is_end()){auto rec=fh_->get_record(rid_,context_);if(eval_conds(rec->data))return;scan_->next();rid_=scan_->rid();}}
    bool is_end() const override { return scan_->is_end(); }
    std::unique_ptr<RmRecord> Next() override { return fh_->get_record(rid_, context_); }
    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }
    Rid &rid() override { return rid_; }
};
