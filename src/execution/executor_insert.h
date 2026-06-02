/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2. */
#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class InsertExecutor : public AbstractExecutor {
   private:
    TabMeta tab_;
    std::vector<Value> values_;
    RmFileHandle *fh_;
    std::string tab_name_;
    Rid rid_;
    SmManager *sm_manager_;

   public:
    InsertExecutor(SmManager *sm, const std::string &tn, std::vector<Value> vs, Context *ctx) {
        sm_manager_ = sm;
        tab_ = sm->db_.get_table(tn);
        values_ = vs;
        tab_name_ = tn;
        if (vs.size() != tab_.cols.size()) throw InvalidValueCountError();
        fh_ = sm->fhs_.at(tn).get();
        context_ = ctx;
    }

    std::unique_ptr<RmRecord> Next() override {
        RmRecord rec(fh_->get_file_hdr().record_size);
        for (size_t i = 0; i < values_.size(); i++) {
            auto &c = tab_.cols[i];
            auto &v = values_[i];
            if (c.type != v.type) {
                if (c.type == TYPE_FLOAT && v.type == TYPE_INT) v.set_float((float)v.int_val);
                else if (c.type == TYPE_INT && v.type == TYPE_FLOAT) v.set_int((int)v.float_val);
                else throw IncompatibleTypeError(coltype2str(c.type), coltype2str(v.type));
            }
            v.init_raw(c.len);
            memcpy(rec.data + c.offset, v.raw->data, c.len);
        }
        for (auto &idx : tab_.indexes) {
            auto ih = sm_manager_->get_ih(tab_name_, idx.cols);
            if (!ih) continue;
            char *k = new char[idx.col_tot_len];
            int off = 0;
            for (size_t j = 0; j < idx.col_num; ++j) {
                memcpy(k + off, rec.data + idx.cols[j].offset, idx.cols[j].len);
                off += idx.cols[j].len;
            }
            std::vector<Rid> ex;
            if (ih->get_value(k, &ex, context_->txn_)) { delete[] k; throw RMDBError("Duplicate entry for unique index"); }
            delete[] k;
        }
        rid_ = fh_->insert_record(rec.data, context_);
        for (auto &idx : tab_.indexes) {
            auto ih = sm_manager_->get_ih(tab_name_, idx.cols);
            if (!ih) continue;
            char *k = new char[idx.col_tot_len];
            int off = 0;
            for (size_t j = 0; j < idx.col_num; ++j) {
                memcpy(k + off, rec.data + idx.cols[j].offset, idx.cols[j].len);
                off += idx.cols[j].len;
            }
            ih->insert_entry(k, rid_, context_->txn_);
            delete[] k;
        }
        return nullptr;
    }
    Rid &rid() override { return rid_; }
};
