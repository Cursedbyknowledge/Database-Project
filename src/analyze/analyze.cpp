/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "analyze.h"

/**
 * @description: 分析器，进行语义分析和查询重写，需要检查不符合语义规定的部分
 * @param {shared_ptr<ast::TreeNode>} parse parser生成的结果集
 * @return {shared_ptr<Query>} Query 
 */
std::shared_ptr<Query> Analyze::do_analyze(std::shared_ptr<ast::TreeNode> parse)
{
    std::shared_ptr<Query> query = std::make_shared<Query>();
    if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(parse))
    {
        // 处理表名
        query->tables = std::move(x->tabs);
        // 检查表是否存在
        for (auto &tab_name : query->tables) {
            if (!sm_manager_->db_.is_table(tab_name)) {
                throw TableNotFoundError(tab_name);
            }
        }

        // 解析别名：将别名引用(c.col)解析为真实表名(customers.col)
        for (auto &sv_col : x->cols) {
            if (!sv_col->tab_name.empty() && x->alias_map.count(sv_col->tab_name))
                sv_col->tab_name = x->alias_map[sv_col->tab_name];
        }
        for (auto &cond : x->conds) {
            if (!cond->lhs->tab_name.empty() && x->alias_map.count(cond->lhs->tab_name))
                cond->lhs->tab_name = x->alias_map[cond->lhs->tab_name];
            if (auto rhs_col = std::dynamic_pointer_cast<ast::Col>(cond->rhs)) {
                if (!rhs_col->tab_name.empty() && x->alias_map.count(rhs_col->tab_name))
                    rhs_col->tab_name = x->alias_map[rhs_col->tab_name];
            }
        }
        // 构建反向别名映射 (real_name -> alias_name) 供EXPLAIN输出
        for (auto &[alias, real] : x->alias_map) {
            query->rev_alias_map_[real] = alias;
        }

        // 处理target list，再target list中添加上表名，例如 a.id
        bool has_agg = false;
        for (auto &sv_sel_col : x->cols) {
            TabCol sel_col = {.tab_name = sv_sel_col->tab_name, .col_name = sv_sel_col->col_name};
            if (sv_sel_col->is_agg) {
                has_agg = true;
                query->agg_funcs.push_back(sv_sel_col->agg_func);
                query->agg_col_names.push_back(sv_sel_col->col_name);
                query->agg_input_idxs.push_back(0);  // 稍后由planner填充
                query->agg_is_star.push_back(sv_sel_col->col_name == "*");
                // 聚合列不需要check_column（列名可能是别名）
            } else {
                query->cols.push_back(sel_col);
            }
        }
        query->has_agg = has_agg;
        
        std::vector<ColMeta> all_cols;
        get_all_cols(query->tables, all_cols);
        if (query->cols.empty() && !has_agg) {
            // select all columns
            query->cols_star_ = true;
            for (auto &col : all_cols) {
                TabCol sel_col = {.tab_name = col.tab_name, .col_name = col.name};
                query->cols.push_back(sel_col);
            }
        } else {
            // infer table name from column name (skip agg cols)
            for (auto &sel_col : query->cols) {
                sel_col = check_column(all_cols, sel_col);  // 列元数据校验
            }
        }
        //处理where条件
        get_clause(x->conds, query->conds);
        check_clause(query->tables, query->conds);
        // GROUP BY: 校验列存在性并推断表名
        if (!x->group_by.empty()) {
            for (auto &gb_col : x->group_by) {
                TabCol tc = check_column(all_cols, {.tab_name = "", .col_name = gb_col});
                query->group_by.push_back(tc.col_name);
            }
        }
        // HAVING: 处理条件（聚合后的过滤）
        if (!x->having.empty()) {
            get_clause(x->having, query->having);
            check_clause(query->tables, query->having);
        }
        // LIMIT
        query->limit_val = x->limit_val;
    } else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(parse)) {
        // 检查表是否存在
        if (!sm_manager_->db_.is_table(x->tab_name)) {
            throw TableNotFoundError(x->tab_name);
        }
        query->tables = {x->tab_name};
        // 检查 set_clauses 中的列是否存在，并存储到 query 中
        auto &tab = sm_manager_->db_.get_table(x->tab_name);
        for (auto &sv_clause : x->set_clauses) {
            // 检查列是否存在
            auto col_it = tab.get_col(sv_clause->col_name);
            SetClause clause;
            clause.lhs = {.tab_name = x->tab_name, .col_name = sv_clause->col_name};
            clause.rhs = convert_sv_value(sv_clause->val);
            // 隐式类型转换：INT↔FLOAT
            if (col_it->type != clause.rhs.type) {
                if (col_it->type == TYPE_FLOAT && clause.rhs.type == TYPE_INT) {
                    clause.rhs.set_float((float)clause.rhs.int_val);
                } else if (col_it->type == TYPE_INT && clause.rhs.type == TYPE_FLOAT) {
                    clause.rhs.set_int((int)clause.rhs.float_val);
                }
            }
            clause.rhs.init_raw(col_it->len);
            query->set_clauses.push_back(clause);
        }
        // 处理 WHERE 条件
        get_clause(x->conds, query->conds);
        check_clause({x->tab_name}, query->conds);

    } else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(parse)) {
        //处理where条件
        get_clause(x->conds, query->conds);
        check_clause({x->tab_name}, query->conds);        
    } else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(parse)) {
        // 处理insert 的values值
        for (auto &sv_val : x->vals) {
            query->values.push_back(convert_sv_value(sv_val));
        }
    } else {
        // do nothing
    }
    query->parse = std::move(parse);
    return query;
}


TabCol Analyze::check_column(const std::vector<ColMeta> &all_cols, TabCol target) {
    if (target.tab_name.empty()) {
        // Table name not specified, infer table name from column name
        std::string tab_name;
        for (auto &col : all_cols) {
            if (col.name == target.col_name) {
                if (!tab_name.empty()) {
                    throw AmbiguousColumnError(target.col_name);
                }
                tab_name = col.tab_name;
            }
        }
        if (tab_name.empty()) {
            throw ColumnNotFoundError(target.col_name);
        }
        target.tab_name = tab_name;
    } else {
        // 检查指定表中是否存在该列
        auto &tab = sm_manager_->db_.get_table(target.tab_name);
        tab.get_col(target.col_name);
        
    }
    return target;
}

void Analyze::get_all_cols(const std::vector<std::string> &tab_names, std::vector<ColMeta> &all_cols) {
    for (auto &sel_tab_name : tab_names) {
        // 这里db_不能写成get_db(), 注意要传指针
        const auto &sel_tab_cols = sm_manager_->db_.get_table(sel_tab_name).cols;
        all_cols.insert(all_cols.end(), sel_tab_cols.begin(), sel_tab_cols.end());
    }
}

void Analyze::get_clause(const std::vector<std::shared_ptr<ast::BinaryExpr>> &sv_conds, std::vector<Condition> &conds) {
    conds.clear();
    for (auto &expr : sv_conds) {
        Condition cond;
        cond.lhs_col = {.tab_name = expr->lhs->tab_name, .col_name = expr->lhs->col_name};
        cond.op = convert_sv_comp_op(expr->op);
        if (auto rhs_val = std::dynamic_pointer_cast<ast::Value>(expr->rhs)) {
            cond.is_rhs_val = true;
            cond.rhs_val = convert_sv_value(rhs_val);
        } else if (auto rhs_col = std::dynamic_pointer_cast<ast::Col>(expr->rhs)) {
            cond.is_rhs_val = false;
            cond.rhs_col = {.tab_name = rhs_col->tab_name, .col_name = rhs_col->col_name};
        }
        conds.push_back(cond);
    }
}

void Analyze::check_clause(const std::vector<std::string> &tab_names, std::vector<Condition> &conds) {
    // auto all_cols = get_all_cols(tab_names);
    std::vector<ColMeta> all_cols;
    get_all_cols(tab_names, all_cols);
    // Get raw values in where clause
    for (auto &cond : conds) {
        // Infer table name from column name
        cond.lhs_col = check_column(all_cols, cond.lhs_col);
        if (!cond.is_rhs_val) {
            cond.rhs_col = check_column(all_cols, cond.rhs_col);
        }
        TabMeta &lhs_tab = sm_manager_->db_.get_table(cond.lhs_col.tab_name);
        auto lhs_col = lhs_tab.get_col(cond.lhs_col.col_name);
        ColType lhs_type = lhs_col->type;
        ColType rhs_type;
        if (cond.is_rhs_val) {
            // 隐式类型转换：INT↔FLOAT
            if (lhs_type != cond.rhs_val.type) {
                if (lhs_type == TYPE_FLOAT && cond.rhs_val.type == TYPE_INT) {
                    cond.rhs_val.set_float((float)cond.rhs_val.int_val);
                } else if (lhs_type == TYPE_INT && cond.rhs_val.type == TYPE_FLOAT) {
                    cond.rhs_val.set_int((int)cond.rhs_val.float_val);
                }
            }
            cond.rhs_val.init_raw(lhs_col->len);
            rhs_type = cond.rhs_val.type;
        } else {
            TabMeta &rhs_tab = sm_manager_->db_.get_table(cond.rhs_col.tab_name);
            auto rhs_col = rhs_tab.get_col(cond.rhs_col.col_name);
            rhs_type = rhs_col->type;
        }
        if (lhs_type != rhs_type) {
            throw IncompatibleTypeError(coltype2str(lhs_type), coltype2str(rhs_type));
        }
    }
}


Value Analyze::convert_sv_value(const std::shared_ptr<ast::Value> &sv_val) {
    Value val;
    if (auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(sv_val)) {
        val.set_int(int_lit->val);
    } else if (auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(sv_val)) {
        val.set_float(float_lit->val);
    } else if (auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(sv_val)) {
        val.set_str(str_lit->val);
    } else {
        throw InternalError("Unexpected sv value type");
    }
    return val;
}

CompOp Analyze::convert_sv_comp_op(ast::SvCompOp op) {
    std::map<ast::SvCompOp, CompOp> m = {
        {ast::SV_OP_EQ, OP_EQ}, {ast::SV_OP_NE, OP_NE}, {ast::SV_OP_LT, OP_LT},
        {ast::SV_OP_GT, OP_GT}, {ast::SV_OP_LE, OP_LE}, {ast::SV_OP_GE, OP_GE},
    };
    return m.at(op);
}
