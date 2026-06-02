/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2. */
#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class DeleteExecutor : public AbstractExecutor {
   private:
    TabMeta tab_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<Rid> rids_;
    std::string tab_name_;
    SmManager *sm_manager_;

   public:
    DeleteExecutor(SmManager *sm, const std::string &tn, std::vector<Condition> cd,
                   std::vector<Rid> rd, Context *ctx)
        : sm_manager_(sm), tab_name_(tn), conds_(cd), rids_(rd) {
        tab_ = sm->db_.get_table(tn);
        fh_ = sm->fhs_.at(tn).get();
        context_ = ctx;
    }

    std::unique_ptr<RmRecord> Next() override {
        if (tab_.indexes.empty()) {
            for (auto &rid : rids_) fh_->delete_record(rid, context_);
        } else {
            for (auto &rid : rids_) {
                auto rec = fh_->get_record(rid, context_);
                for (auto &idx : tab_.indexes) {
                    auto ih = sm_manager_->get_ih(tab_name_, idx.cols);
                    if (!ih) continue;
                    char *k = new char[idx.col_tot_len];
                    int off = 0;
                    for (size_t j = 0; j < idx.col_num; ++j) {
                        memcpy(k + off, rec->data + idx.cols[j].offset, idx.cols[j].len);
                        off += idx.cols[j].len;
                    }
                    ih->delete_entry(k, context_->txn_);
                    delete[] k;
                }
                fh_->delete_record(rid, context_);
            }
        }
        return nullptr;
    }
    Rid &rid() override { return _abstract_rid; }
};
