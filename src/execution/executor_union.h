#pragma once

#include <algorithm>
#include <cstring>
#include <vector>
#include <memory>

#include "execution/executor_abstract.h"
#include "parser/ast.h"
#include "system/sm.h"
#include "record/rm_scan.h"

class UnionExecutor : public AbstractExecutor {
public:
    UnionExecutor(SmManager *sm_manager,
                  std::shared_ptr<ast::UnionStmt> union_stmt,
                  std::vector<ColMeta> output_cols,
                  std::vector<TabCol> sort_cols,
                  std::vector<bool> sort_desc)
        : sm_manager_(sm_manager),
          union_stmt_(std::move(union_stmt)),
          output_cols_(std::move(output_cols)),
          sort_cols_(std::move(sort_cols)),
          sort_desc_(std::move(sort_desc)) {
        // 计算元组长度
        tuple_len_ = 0;
        for (auto &col : output_cols_) {
            tuple_len_ += col.len;
        }
        cur_idx_ = 0;
    }

    void beginTuple() override {
        tuples_.clear();
        cur_idx_ = 0;

        // 收集每个子查询的结果
        for (auto &sub_sel : union_stmt_->sub_selects) {
            collect_sub_query(sub_sel);
        }

        // 排序去重：先按全部字节排序
        std::sort(tuples_.begin(), tuples_.end(),
            [this](const std::vector<char> &a, const std::vector<char> &b) {
                return memcmp(a.data(), b.data(), tuple_len_) < 0;
            });
        // 相邻去重
        auto last = std::unique(tuples_.begin(), tuples_.end(),
            [this](const std::vector<char> &a, const std::vector<char> &b) {
                return memcmp(a.data(), b.data(), tuple_len_) == 0;
            });
        tuples_.erase(last, tuples_.end());

        // 如果有 ORDER BY，按指定列排序
        if (!sort_cols_.empty()) {
            // 预计算排序列的偏移和长度
            struct SortKey { int offset; int len; ColType type; bool desc; };
            std::vector<SortKey> keys;
            for (size_t i = 0; i < sort_cols_.size(); i++) {
                for (auto &col : output_cols_) {
                    if (col.name == sort_cols_[i].col_name) {
                        keys.push_back({col.offset, col.len, col.type,
                                       i < sort_desc_.size() ? sort_desc_[i] : false});
                        break;
                    }
                }
            }

            std::sort(tuples_.begin(), tuples_.end(),
                [&keys](const std::vector<char> &a, const std::vector<char> &b) {
                    for (auto &k : keys) {
                        int cmp = 0;
                        if (k.type == TYPE_INT) {
                            int va, vb;
                            memcpy(&va, a.data() + k.offset, sizeof(int));
                            memcpy(&vb, b.data() + k.offset, sizeof(int));
                            cmp = (va < vb) ? -1 : (va > vb) ? 1 : 0;
                        } else if (k.type == TYPE_FLOAT) {
                            float va, vb;
                            memcpy(&va, a.data() + k.offset, sizeof(float));
                            memcpy(&vb, b.data() + k.offset, sizeof(float));
                            cmp = (va < vb) ? -1 : (va > vb) ? 1 : 0;
                        } else {
                            cmp = memcmp(a.data() + k.offset, b.data() + k.offset, k.len);
                        }
                        if (cmp != 0) {
                            return k.desc ? (cmp > 0) : (cmp < 0);
                        }
                    }
                    return false;
                });
        }
    }

    void nextTuple() override {
        cur_idx_++;
    }

    bool is_end() const override {
        return cur_idx_ >= tuples_.size();
    }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        auto rec = std::make_unique<RmRecord>(tuple_len_);
        memcpy(rec->data, tuples_[cur_idx_].data(), tuple_len_);
        return rec;
    }

    const std::vector<ColMeta> &cols() const override {
        return output_cols_;
    }

    size_t tupleLen() const override {
        return tuple_len_;
    }

    Rid &rid() override { return _abstract_rid; }

    std::string getType() override { return "UnionExecutor"; }

private:
    SmManager *sm_manager_;
    std::shared_ptr<ast::UnionStmt> union_stmt_;
    std::vector<ColMeta> output_cols_;
    std::vector<TabCol> sort_cols_;
    std::vector<bool> sort_desc_;

    std::vector<std::vector<char>> tuples_;
    size_t tuple_len_;
    size_t cur_idx_;

    void collect_sub_query(const std::shared_ptr<ast::SelectStmt> &sub_sel) {
        // 获取子查询的源表列元数据
        std::vector<ColMeta> src_cols;
        for (auto &tab_name : sub_sel->tabs) {
            auto &tab_meta = sm_manager_->db_.get_table(tab_name);
            src_cols.insert(src_cols.end(), tab_meta.cols.begin(), tab_meta.cols.end());
        }

        // 确定子查询的输出列（如果 cols 为空则是 SELECT *）
        std::vector<ColMeta> sub_output;
        if (sub_sel->cols.empty()) {
            sub_output = src_cols;
        } else {
            for (auto &sv_col : sub_sel->cols) {
                std::string tn = sv_col->tab_name;
                std::string cn = sv_col->col_name;
                // 如果未指定表名，推断
                if (tn.empty()) {
                    for (auto &c : src_cols) {
                        if (c.name == cn) { tn = c.tab_name; break; }
                    }
                }
                for (auto &c : src_cols) {
                    if (c.tab_name == tn && c.name == cn) {
                        sub_output.push_back(c);
                        break;
                    }
                }
            }
        }

        // 扫描表，收集记录
        // 对于单表子查询（最常见情况）
        std::string main_tab = sub_sel->tabs[0];
        auto fh = sm_manager_->fhs_.at(main_tab).get();

        for (RmScan scan(fh); !scan.is_end(); scan.next()) {
            auto rid = scan.rid();
            auto rec = fh->get_record(rid, nullptr);

            // 应用 WHERE 条件（简单过滤）
            if (!sub_sel->conds.empty()) {
                if (!eval_conds(rec.get(), src_cols, sub_sel->conds)) {
                    continue;
                }
            }

            // 构建输出元组：根据 output_cols_ 进行类型提升
            std::vector<char> tuple(tuple_len_, 0);
            for (size_t i = 0; i < output_cols_.size() && i < sub_output.size(); i++) {
                const ColMeta &src = sub_output[i];
                const ColMeta &dst = output_cols_[i];

                char *src_data = rec->data + src.offset;
                char *dst_data = tuple.data() + dst.offset;

                if (src.type == dst.type) {
                    // 同类型直接复制（CHAR 可能需要补零）
                    int copy_len = std::min(src.len, dst.len);
                    memcpy(dst_data, src_data, copy_len);
                } else if (src.type == TYPE_INT && dst.type == TYPE_FLOAT) {
                    // INT → FLOAT 提升
                    int int_val;
                    memcpy(&int_val, src_data, sizeof(int));
                    float float_val = (float)int_val;
                    memcpy(dst_data, &float_val, sizeof(float));
                } else if (src.type == TYPE_FLOAT && dst.type == TYPE_INT) {
                    // FLOAT → INT 截断（不太可能发生）
                    float float_val;
                    memcpy(&float_val, src_data, sizeof(float));
                    int int_val = (int)float_val;
                    memcpy(dst_data, &int_val, sizeof(int));
                }
            }
            tuples_.push_back(std::move(tuple));
        }
    }

    // 简单条件求值
    bool eval_conds(const RmRecord *rec, const std::vector<ColMeta> &cols,
                    const std::vector<std::shared_ptr<ast::BinaryExpr>> &conds) {
        for (auto &cond : conds) {
            // 查找左列
            std::string lhs_tab = cond->lhs->tab_name;
            std::string lhs_col = cond->lhs->col_name;
            const ColMeta *lhs_meta = nullptr;
            for (auto &c : cols) {
                if ((lhs_tab.empty() || c.tab_name == lhs_tab) && c.name == lhs_col) {
                    lhs_meta = &c;
                    break;
                }
            }
            if (!lhs_meta) continue;

            // 获取左值
            char *lhs_data = rec->data + lhs_meta->offset;

            // 右值必须是常量
            auto rhs_val = std::dynamic_pointer_cast<ast::Value>(cond->rhs);
            if (!rhs_val) continue;  // 列-列比较暂不处理

            int cmp = 0;
            if (auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(rhs_val)) {
                if (lhs_meta->type == TYPE_INT) {
                    int lv; memcpy(&lv, lhs_data, sizeof(int));
                    cmp = (lv < int_lit->val) ? -1 : (lv > int_lit->val) ? 1 : 0;
                } else if (lhs_meta->type == TYPE_FLOAT) {
                    float lv; memcpy(&lv, lhs_data, sizeof(float));
                    float rv = (float)int_lit->val;
                    cmp = (lv < rv) ? -1 : (lv > rv) ? 1 : 0;
                }
            } else if (auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(rhs_val)) {
                if (lhs_meta->type == TYPE_FLOAT) {
                    float lv; memcpy(&lv, lhs_data, sizeof(float));
                    cmp = (lv < float_lit->val) ? -1 : (lv > float_lit->val) ? 1 : 0;
                } else if (lhs_meta->type == TYPE_INT) {
                    int lv; memcpy(&lv, lhs_data, sizeof(int));
                    float rv = float_lit->val;
                    cmp = ((float)lv < rv) ? -1 : ((float)lv > rv) ? 1 : 0;
                }
            } else if (auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(rhs_val)) {
                if (lhs_meta->type == TYPE_STRING) {
                    std::string lv(lhs_data, strnlen(lhs_data, lhs_meta->len));
                    cmp = lv.compare(str_lit->val);
                }
            }

            bool pass = false;
            switch (cond->op) {
                case ast::SV_OP_EQ: pass = (cmp == 0); break;
                case ast::SV_OP_NE: pass = (cmp != 0); break;
                case ast::SV_OP_LT: pass = (cmp < 0); break;
                case ast::SV_OP_GT: pass = (cmp > 0); break;
                case ast::SV_OP_LE: pass = (cmp <= 0); break;
                case ast::SV_OP_GE: pass = (cmp >= 0); break;
            }
            if (!pass) return false;
        }
        return true;
    }
};
