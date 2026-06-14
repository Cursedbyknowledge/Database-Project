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
#include "execution_common.h"

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
        auto ix_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index_col_names_);
        auto ih = sm_manager_->ihs_.at(ix_name).get();
        Iid start = ih->leaf_begin();
        Iid end   = ih->leaf_end();
        // 等值条件：lower_bound定位起点, lower_bound(key+1)定位终点
        for (auto &cond : conds_) {
            if (!cond.is_rhs_val || cond.lhs_col.tab_name != tab_name_)
                continue;
            if (cond.op != OP_EQ) continue;
            int col_tot = index_meta_.col_tot_len;
            char *key_buf = new char[col_tot];
            char *key_end = new char[col_tot];
            memset(key_buf, 0, col_tot);
            memcpy(key_end, key_buf, col_tot);
            int off = 0;
            for (auto &icol : index_meta_.cols) {
                if (icol.name == cond.lhs_col.col_name) {
                    memcpy(key_buf + off, cond.rhs_val.raw->data, icol.len);
                    memcpy(key_end + off, cond.rhs_val.raw->data, icol.len);
                    // 构造key+1作为终点（INT:值+1; FLOAT:值+1; CHAR:末字节+1）
                    if (icol.type == TYPE_INT) {
                        (*(int*)(key_end + off))++;
                    } else if (icol.type == TYPE_FLOAT) {
                        float f = *(float*)(key_end + off);
                        *(float*)(key_end + off) = f + 1.0f;
                    } else {
                        // CHAR: 末非0xFF字节+1
                        for (int k = icol.len - 1; k >= 0; k--) {
                            unsigned char &c = (unsigned char &)key_end[off + k];
                            if (c < 0xFF) { c++; break; }
                            c = 0;
                        }
                    }
                    break;
                }
                off += icol.len;
            }
            start = ih->lower_bound(key_buf);
            end   = ih->lower_bound(key_end);
            delete[] key_buf;
            delete[] key_end;
            break;
        }
        scan_ = std::make_unique<IxScan>(ih, start, end, sm_manager_->get_bpm());
    }

    void nextTuple() override {
        if (!scan_->is_end()) scan_->next();
    }

    std::unique_ptr<RmRecord> Next() override {
        if (scan_->is_end()) return nullptr;
        rows_++;  // 计数扫描行数
        rid_ = scan_->rid();
        try {
            auto rec = fh_->get_record(rid_, context_);
            if (rec != nullptr) {
                // 检查是否满足过滤条件
                for (auto& cond : fed_conds_) {
                    if (!eval_cond(rec->data, cond, cols_)) {
                        return nullptr;
                    }
                }
                return rec;
            }
        } catch (RecordNotFoundError &e) {
            // 记录已被删除
        } catch (PageNotExistError &e) {
            // 索引条目指向不存在的页面
        } catch (RMDBError &e) {
            // 其他错误，跳过
        }
        return nullptr;
    }

    bool is_end() const override { return scan_ ? scan_->is_end() : true; }

    const std::vector<ColMeta> &cols() const override { return cols_; }
    size_t tupleLen() const override { return len_; }

    Rid &rid() override { return rid_; }
};
