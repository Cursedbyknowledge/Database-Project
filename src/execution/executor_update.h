/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2. */
#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class UpdateExecutor : public AbstractExecutor {
   private:
    TabMeta tab_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<Rid> rids_;
    std::string tab_name_;
    std::vector<SetClause> set_clauses_;
    SmManager *sm_manager_;

    void build_key(char *kbuf, const IndexMeta &idx, const char *rec) {
        int off = 0;
        for (size_t j = 0; j < idx.col_num; ++j) {
            memcpy(kbuf + off, rec + idx.cols[j].offset, idx.cols[j].len);
            off += idx.cols[j].len;
        }
    }

   public:
    UpdateExecutor(SmManager *sm, const std::string &tn, std::vector<SetClause> sc,
                   std::vector<Condition> cd, std::vector<Rid> rd, Context *ctx)
        : sm_manager_(sm), tab_name_(tn), set_clauses_(sc), fh_(sm->fhs_.at(tn).get()),
          conds_(cd), rids_(rd) {
        tab_ = sm->db_.get_table(tn);
        context_ = ctx;
    }

    std::unique_ptr<RmRecord> Next() override {
        for (auto &rid : rids_) {
            if (tab_.indexes.empty()) {
                // 无索引：原始逻辑
                auto rec = fh_->get_record(rid, context_);
                for (auto &sc : set_clauses_) {
                    auto cm = tab_.get_col(sc.lhs.col_name);
                    sc.rhs.init_raw(cm->len);
                    memcpy(rec->data + cm->offset, sc.rhs.raw->data, cm->len);
                    if (sc.rhs.raw) sc.rhs.raw.reset();
                }
                fh_->update_record(rid, rec->data, context_);
                continue;
            }
            // 有索引：五阶段流程
            auto rec = fh_->get_record(rid, context_);
            std::vector<char> nd(fh_->get_file_hdr().record_size);
            memcpy(nd.data(), rec->data, fh_->get_file_hdr().record_size);
            for (auto &sc : set_clauses_) {
                auto cm = tab_.get_col(sc.lhs.col_name);
                sc.rhs.init_raw(cm->len);
                memcpy(nd.data() + cm->offset, sc.rhs.raw->data, cm->len);
                if (sc.rhs.raw) sc.rhs.raw.reset();
            }
            // 唯一性检查
            for (auto &idx : tab_.indexes) {
                auto ih = sm_manager_->get_ih(tab_name_, idx.cols);
                if (!ih) continue;
                char *nk = new char[idx.col_tot_len];
                build_key(nk, idx, nd.data());
                std::vector<Rid> ex;
                if (ih->get_value(nk, &ex, context_->txn_)) {
                    for (auto &r : ex) {
                        if (r.page_no != rid.page_no || r.slot_no != rid.slot_no) {
                            delete[] nk;
                            throw RMDBError("Duplicate entry for unique index");
                        }
                    }
                }
                delete[] nk;
            }
            // 删旧索引
            for (auto &idx : tab_.indexes) {
                auto ih = sm_manager_->get_ih(tab_name_, idx.cols);
                if (!ih) continue;
                char *ok = new char[idx.col_tot_len];
                build_key(ok, idx, rec->data);
                ih->delete_entry(ok, context_->txn_);
                delete[] ok;
            }
            // 插新索引
            for (auto &idx : tab_.indexes) {
                auto ih = sm_manager_->get_ih(tab_name_, idx.cols);
                if (!ih) continue;
                char *nk = new char[idx.col_tot_len];
                build_key(nk, idx, nd.data());
                ih->insert_entry(nk, rid, context_->txn_);
                delete[] nk;
            }
            fh_->update_record(rid, nd.data(), context_);
        }
        return nullptr;
    }
    Rid &rid() override { return _abstract_rid; }
};
