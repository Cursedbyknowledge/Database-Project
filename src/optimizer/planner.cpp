/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "planner.h"

#include <memory>
#include <set>

#include "execution/executor_delete.h"
#include "execution/executor_index_scan.h"
#include "execution/executor_insert.h"
#include "execution/executor_nestedloop_join.h"
#include "execution/executor_projection.h"
#include "execution/executor_seq_scan.h"
#include "execution/executor_update.h"
#include "index/ix.h"
#include "record_printer.h"

// 索引匹配规则：最左前缀匹配（赛题要求）
// 匹配所有等值条件以及第一个范围条件
bool Planner::get_index_cols(std::string tab_name, std::vector<Condition> curr_conds, std::vector<std::string>& index_col_names) {
    index_col_names.clear();
    for(auto& cond: curr_conds) {
        if(cond.is_rhs_val && cond.lhs_col.tab_name.compare(tab_name) == 0) {
            if (cond.op == OP_EQ) {
                index_col_names.push_back(cond.lhs_col.col_name);
            } else {
                // 第一个范围条件也可以利用索引
                index_col_names.push_back(cond.lhs_col.col_name);
                break;
            }
        }
    }
    if (index_col_names.empty()) return false;
    TabMeta& tab = sm_manager_->db_.get_table(tab_name);
    if(tab.is_index(index_col_names)) return true;
    // 最左前缀匹配：逐列回退
    while (index_col_names.size() > 1) {
        index_col_names.pop_back();
        if (tab.is_index(index_col_names)) return true;
    }
    return index_col_names.size() >= 1 && tab.is_index(index_col_names);
}

/**
 * @brief 表算子条件谓词生成
 *
 * @param conds 条件
 * @param tab_names 表名
 * @return std::vector<Condition>
 */
std::vector<Condition> pop_conds(std::vector<Condition> &conds, std::string tab_names) {
    // auto has_tab = [&](const std::string &tab_name) {
    //     return std::find(tab_names.begin(), tab_names.end(), tab_name) != tab_names.end();
    // };
    std::vector<Condition> solved_conds;
    auto it = conds.begin();
    while (it != conds.end()) {
        if ((tab_names.compare(it->lhs_col.tab_name) == 0 && it->is_rhs_val) || (it->lhs_col.tab_name.compare(it->rhs_col.tab_name) == 0)) {
            solved_conds.emplace_back(std::move(*it));
            it = conds.erase(it);
        } else {
            it++;
        }
    }
    return solved_conds;
}

int push_conds(Condition *cond, std::shared_ptr<Plan> plan)
{
    if(auto x = std::dynamic_pointer_cast<ScanPlan>(plan))
    {
        if(x->tab_name_.compare(cond->lhs_col.tab_name) == 0) {
            return 1;
        } else if(x->tab_name_.compare(cond->rhs_col.tab_name) == 0){
            return 2;
        } else {
            return 0;
        }
    }
    else if(auto x = std::dynamic_pointer_cast<JoinPlan>(plan))
    {
        int left_res = push_conds(cond, x->left_);
        // 条件已经下推到左子节点
        if(left_res == 3){
            return 3;
        }
        int right_res = push_conds(cond, x->right_);
        // 条件已经下推到右子节点
        if(right_res == 3){
            return 3;
        }
        // 左子节点或右子节点有一个没有匹配到条件的列
        if(left_res == 0 || right_res == 0) {
            return left_res + right_res;
        }
        // 左子节点匹配到条件的右边
        if(left_res == 2) {
            // 需要将左右两边的条件变换位置
            std::map<CompOp, CompOp> swap_op = {
                {OP_EQ, OP_EQ}, {OP_NE, OP_NE}, {OP_LT, OP_GT}, {OP_GT, OP_LT}, {OP_LE, OP_GE}, {OP_GE, OP_LE},
            };
            std::swap(cond->lhs_col, cond->rhs_col);
            cond->op = swap_op.at(cond->op);
        }
        x->conds_.emplace_back(std::move(*cond));
        return 3;
    }
    return false;
}

std::shared_ptr<Plan> pop_scan(int *scantbl, std::string table, std::vector<std::string> &joined_tables, 
                std::vector<std::shared_ptr<Plan>> plans)
{
    for (size_t i = 0; i < plans.size(); i++) {
        auto x = std::dynamic_pointer_cast<ScanPlan>(plans[i]);
        if(x->tab_name_.compare(table) == 0)
        {
            scantbl[i] = 1;
            joined_tables.emplace_back(x->tab_name_);
            return plans[i];
        }
    }
    return nullptr;
}


std::shared_ptr<Query> Planner::logical_optimization(std::shared_ptr<Query> query, Context *context)
{
    // 逻辑优化：谓词规范化与去重
    if (!query->conds.empty()) {
        // 1. 移除重复条件
        std::vector<Condition> deduped;
        for (auto &cond : query->conds) {
            bool is_dup = false;
            for (auto &existing : deduped) {
                if (cond.lhs_col.tab_name == existing.lhs_col.tab_name &&
                    cond.lhs_col.col_name == existing.lhs_col.col_name &&
                    cond.op == existing.op &&
                    cond.is_rhs_val == existing.is_rhs_val) {
                    if (cond.is_rhs_val) {
                        if (cond.rhs_val.type == existing.rhs_val.type &&
                            memcmp(cond.rhs_val.raw->data, existing.rhs_val.raw->data, existing.rhs_val.raw->size) == 0) {
                            is_dup = true;
                            break;
                        }
                    } else {
                        if (cond.rhs_col.tab_name == existing.rhs_col.tab_name &&
                            cond.rhs_col.col_name == existing.rhs_col.col_name) {
                            is_dup = true;
                            break;
                        }
                    }
                }
            }
            if (!is_dup) {
                deduped.push_back(cond);
            }
        }
        // 2. 将单表+常量条件排到前面（便于 make_one_rel 的 pop_conds 优先处理）
        std::vector<Condition> single_tab, join_conds;
        for (auto &cond : deduped) {
            if (cond.is_rhs_val) {
                single_tab.push_back(cond);
            } else {
                join_conds.push_back(cond);
            }
        }
        query->conds.clear();
        query->conds.insert(query->conds.end(), single_tab.begin(), single_tab.end());
        query->conds.insert(query->conds.end(), join_conds.begin(), join_conds.end());
    }
    return query;
}

std::shared_ptr<Plan> Planner::physical_optimization(std::shared_ptr<Query> query, Context *context)
{
    std::shared_ptr<Plan> plan = make_one_rel(query);
    
    // 其他物理优化

    // 处理orderby
    plan = generate_sort_plan(query, std::move(plan)); 

    return plan;
}



std::shared_ptr<Plan> Planner::make_one_rel(std::shared_ptr<Query> query)
{
    auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse);
    std::vector<std::string> tables = query->tables;
    // // Scan table , 生成表算子列表tab_nodes
    std::vector<std::shared_ptr<Plan>> table_scan_executors(tables.size());
    for (size_t i = 0; i < tables.size(); i++) {
        auto curr_conds = pop_conds(query->conds, tables[i]);
        // int index_no = get_indexNo(tables[i], curr_conds);
        std::vector<std::string> index_col_names;
        bool index_exist = get_index_cols(tables[i], curr_conds, index_col_names);
        if (index_exist == false) {  // 该表没有索引
            index_col_names.clear();
            table_scan_executors[i] = 
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, tables[i], curr_conds, index_col_names);
        } else {  // 存在索引
            table_scan_executors[i] =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, tables[i], curr_conds, index_col_names);
        }
    }
    // 只有一个表，不需要join。
    if(tables.size() == 1)
    {
        return table_scan_executors[0];
    }
    // 获取where条件
    auto conds = std::move(query->conds);
    // 谓词下推：将单表过滤条件(is_rhs_val)推至对应ScanPlan的fed_conds_
    auto it_filt = conds.begin();
    while (it_filt != conds.end()) {
        if (it_filt->is_rhs_val) {
            auto sp = std::dynamic_pointer_cast<ScanPlan>(table_scan_executors[0]);
            bool pushed = false;
            for (size_t i = 0; i < tables.size(); i++) {
                sp = std::dynamic_pointer_cast<ScanPlan>(table_scan_executors[i]);
                if (sp && sp->tab_name_ == it_filt->lhs_col.tab_name) {
                    sp->fed_conds_.push_back(*it_filt);
                    pushed = true;
                    break;
                }
            }
            if (pushed) {
                it_filt = conds.erase(it_filt);
            } else {
                ++it_filt;
            }
        } else {
            ++it_filt;
        }
    }
    std::shared_ptr<Plan> table_join_executors;
    
    int scantbl[tables.size()];
    for(size_t i = 0; i < tables.size(); i++)
    {
        scantbl[i] = -1;
    }
    // 假设在ast中已经添加了jointree，这里需要修改的逻辑是，先处理jointree，然后再考虑剩下的部分
    if(conds.size() >= 1)
    {
        // 有连接条件

        // 根据连接条件，生成第一层join
        std::vector<std::string> joined_tables(tables.size());
        auto it = conds.begin();
        while (it != conds.end()) {
            std::shared_ptr<Plan> left , right;
            left = pop_scan(scantbl, it->lhs_col.tab_name, joined_tables, table_scan_executors);
            right = pop_scan(scantbl, it->rhs_col.tab_name, joined_tables, table_scan_executors);
            std::vector<Condition> join_conds{*it};
            //建立join
            // 判断使用哪种join方式
            if(enable_nestedloop_join && enable_sortmerge_join) {
                // 默认nested loop join
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left), std::move(right), join_conds);
            } else if(enable_nestedloop_join) {
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left), std::move(right), join_conds);
            } else if(enable_sortmerge_join) {
                table_join_executors = std::make_shared<JoinPlan>(T_SortMerge, std::move(left), std::move(right), join_conds);
            } else {
                // error
                throw RMDBError("No join executor selected!");
            }

            // table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left), std::move(right), join_conds);
            it = conds.erase(it);
            break;
        }
        // 根据连接条件，生成第2-n层join
        it = conds.begin();
        while (it != conds.end()) {
            std::shared_ptr<Plan> left_need_to_join_executors = nullptr;
            std::shared_ptr<Plan> right_need_to_join_executors = nullptr;
            bool isneedreverse = false;
            if (std::find(joined_tables.begin(), joined_tables.end(), it->lhs_col.tab_name) == joined_tables.end()) {
                left_need_to_join_executors = pop_scan(scantbl, it->lhs_col.tab_name, joined_tables, table_scan_executors);
            }
            if (std::find(joined_tables.begin(), joined_tables.end(), it->rhs_col.tab_name) == joined_tables.end()) {
                right_need_to_join_executors = pop_scan(scantbl, it->rhs_col.tab_name, joined_tables, table_scan_executors);
                isneedreverse = true;
            } 

            // 辅助：从新ScanPlan获取表名
            auto get_tab_name = [](std::shared_ptr<Plan> p) -> std::string {
                if (auto sp = std::dynamic_pointer_cast<ScanPlan>(p)) return sp->tab_name_;
                return "";
            };
            if(left_need_to_join_executors != nullptr && right_need_to_join_executors != nullptr) {
                // 两表直接join：当前条件 + 仅合并与这两个新表相关的已有cond
                std::vector<Condition> merged_conds{*it};
                std::string tab_a = get_tab_name(left_need_to_join_executors);
                std::string tab_b = get_tab_name(right_need_to_join_executors);
                if (auto existing_jp = std::dynamic_pointer_cast<JoinPlan>(table_join_executors)) {
                    for (auto &ec : existing_jp->conds_) {
                        if (ec.lhs_col.tab_name == tab_a || ec.lhs_col.tab_name == tab_b ||
                            ec.rhs_col.tab_name == tab_a || ec.rhs_col.tab_name == tab_b)
                            merged_conds.push_back(ec);
                    }
                }
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, 
                                                                    std::move(left_need_to_join_executors), 
                                                                    std::move(right_need_to_join_executors), 
                                                                    std::move(merged_conds));
            } else if(left_need_to_join_executors != nullptr || right_need_to_join_executors != nullptr) {
                if(isneedreverse) {
                    std::map<CompOp, CompOp> swap_op = {
                        {OP_EQ, OP_EQ}, {OP_NE, OP_NE}, {OP_LT, OP_GT}, {OP_GT, OP_LT}, {OP_LE, OP_GE}, {OP_GE, OP_LE},
                    };
                    std::swap(it->lhs_col, it->rhs_col);
                    it->op = swap_op.at(it->op);
                    left_need_to_join_executors = std::move(right_need_to_join_executors);
                }
                // 单表join：当前条件 + 仅合并与新表相关的已有cond
                std::vector<Condition> merged_conds{*it};
                std::string new_tab = get_tab_name(left_need_to_join_executors);
                if (auto existing_jp = std::dynamic_pointer_cast<JoinPlan>(table_join_executors)) {
                    for (auto &ec : existing_jp->conds_) {
                        if (ec.lhs_col.tab_name == new_tab || ec.rhs_col.tab_name == new_tab)
                            merged_conds.push_back(ec);
                    }
                }
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left_need_to_join_executors), 
                                                                    std::move(table_join_executors), std::move(merged_conds));
            } else {
                push_conds(std::move(&(*it)), table_join_executors);
            }
            it = conds.erase(it);
        }
    } else {
        table_join_executors = table_scan_executors[0];
        scantbl[0] = 1;
    }

    //连接剩余表（保留已有conds，不倒空）
    for (size_t i = 0; i < tables.size(); i++) {
        if(scantbl[i] == -1) {
            auto existing_conds = (table_join_executors) 
                ? (std::dynamic_pointer_cast<JoinPlan>(table_join_executors))->conds_
                : std::vector<Condition>();
            table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(table_scan_executors[i]), 
                                                    std::move(table_join_executors), std::move(existing_conds));
        }
    }

    return table_join_executors;

}


std::shared_ptr<Plan> Planner::generate_sort_plan(std::shared_ptr<Query> query, std::shared_ptr<Plan> plan)
{
    auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse);
    if(!x->has_sort) {
        return plan;
    }
    std::vector<std::string> tables = query->tables;
    std::vector<ColMeta> all_cols;
    for (auto &sel_tab_name : tables) {
        // 这里db_不能写成get_db(), 注意要传指针
        const auto &sel_tab_cols = sm_manager_->db_.get_table(sel_tab_name).cols;
        all_cols.insert(all_cols.end(), sel_tab_cols.begin(), sel_tab_cols.end());
    }
    TabCol sel_col;
    bool found_gs = false;
    for (auto &col : all_cols) {
        if(col.name.compare(x->order->cols->col_name) == 0 ) {
            sel_col = {.tab_name = col.tab_name, .col_name = col.name};
            found_gs = true;
            break;
        }
    }
    if (!found_gs) return plan;
    return std::make_shared<SortPlan>(T_Sort, std::move(plan), sel_col, 
                                    x->order->orderby_dir == ast::OrderBy_DESC);
}


/**
 * @brief select plan 生成
 *
 * @param sel_cols select plan 选取的列
 * @param tab_names select plan 目标的表
 * @param conds select plan 选取条件
 */

// 投影下推辅助函数：递归在Join下方为每表插入Project节点
static std::shared_ptr<Plan> pushdown_projection_impl(
    std::shared_ptr<Plan> plan,
    const std::vector<TabCol>& sel_cols,
    const std::vector<Condition>& all_conds) {
    
    if (auto jp = std::dynamic_pointer_cast<JoinPlan>(plan)) {
        // 递归处理子树
        jp->left_ = pushdown_projection_impl(jp->left_, sel_cols, all_conds);
        jp->right_ = pushdown_projection_impl(jp->right_, sel_cols, all_conds);
        return plan;
    }
    if (auto sp = std::dynamic_pointer_cast<ScanPlan>(plan)) {
        auto &tab_cols = sp->cols_;
        // 收集该表需要的列：从sel_cols筛选 + 从join条件提取key
        std::set<std::string> needed_names;
        for (auto &col : sel_cols) {
            if (col.tab_name == sp->tab_name_) {
                needed_names.insert(col.col_name);
            }
        }
        // 加入所有join条件中该表参与的列
        for (auto &cond : all_conds) {
            if (!cond.is_rhs_val) { // join条件
                if (cond.lhs_col.tab_name == sp->tab_name_)
                    needed_names.insert(cond.lhs_col.col_name);
                if (cond.rhs_col.tab_name == sp->tab_name_)
                    needed_names.insert(cond.rhs_col.col_name);
            }
        }
        // 如果不需要精简（SELECT * 或无相关条件），跳过
        if (needed_names.empty() || needed_names.size() >= tab_cols.size()) {
            return plan;
        }
        // 构建Project的sel_cols（保持原始列顺序）
        std::vector<TabCol> proj_cols;
        for (auto &col_meta : tab_cols) {
            if (needed_names.count(col_meta.name)) {
                proj_cols.push_back({.tab_name = sp->tab_name_, .col_name = col_meta.name});
            }
        }
        if (proj_cols.size() >= tab_cols.size()) return plan;
        // 插入Project节点
        return std::make_shared<ProjectionPlan>(T_Projection, std::move(plan), std::move(proj_cols));
    }
    return plan;
}

std::shared_ptr<Plan> Planner::generate_select_plan(std::shared_ptr<Query> query, Context *context) {
    // 保存反向别名映射到context，供EXPLAIN输出使用
    context->rev_alias_map_ = query->rev_alias_map_;
    //逻辑优化
    query = logical_optimization(std::move(query), context);

    std::shared_ptr<Plan> plannerRoot = physical_optimization(query, context);

    // 聚合查询特殊处理
    if (query->has_agg) {
        // 提取底层 ScanPlan（可能被 SortPlan 包裹）
        std::shared_ptr<ScanPlan> scan;
        if (auto sp = std::dynamic_pointer_cast<ScanPlan>(plannerRoot)) {
            scan = sp;
        } else if (auto sort = std::dynamic_pointer_cast<SortPlan>(plannerRoot)) {
            scan = std::dynamic_pointer_cast<ScanPlan>(sort->subplan_);
            plannerRoot = sort->subplan_;  // 聚合场景：去掉 sort, 聚合后再排序
        }
        std::vector<ColMeta> out_cols;
        std::vector<ColMeta> empty_cols;
        auto &scan_cols = (scan) ? scan->cols_ : empty_cols;

        // 计算 GROUP BY 列索引
        std::vector<size_t> group_idxs;
        for (auto &gb_name : query->group_by) {
            for (size_t j = 0; j < scan_cols.size(); j++) {
                if (scan_cols[j].name == gb_name) {
                    group_idxs.push_back(j);
                    break;
                }
            }
        }

        // 构建输出列元数据：先 GROUP BY 列，再聚合列
        // GROUP BY 列加入输出
        for (auto &gb_name : query->group_by) {
            for (auto &sc : scan_cols) {
                if (sc.name == gb_name) {
                    ColMeta cm = sc;
                    cm.tab_name = "";
                    out_cols.push_back(cm);
                    break;
                }
            }
        }
        // 聚合列加入输出
        auto sel_stmt = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse);
        int agg_idx = 0;
        if (sel_stmt) {
            for (auto &c : sel_stmt->cols) {
                if (!c->is_agg) continue;
                std::string input_col = c->tab_name.empty() ? c->col_name : c->tab_name;
                size_t input_idx = 0;
                if (input_col != "*") {
                    for (size_t j = 0; j < scan_cols.size(); j++) {
                        if (scan_cols[j].name == input_col) { input_idx = j; break; }
                    }
                }
                query->agg_input_idxs[agg_idx] = input_idx;
                ColMeta cm;
                cm.tab_name = "";
                cm.name = c->col_name;
                if (c->agg_func == "COUNT") {
                    cm.type = TYPE_INT; cm.len = sizeof(int);
                } else if (input_col != "*" && input_idx < scan_cols.size()) {
                    cm.type = (c->agg_func == "AVG") ? TYPE_FLOAT : scan_cols[input_idx].type;
                    cm.len = (c->agg_func == "AVG") ? (int)sizeof(float) : scan_cols[input_idx].len;
                } else {
                    cm.type = TYPE_FLOAT; cm.len = sizeof(float);
                }
                out_cols.push_back(cm);
                agg_idx++;
            }
        }
        // 插入AggPlan
        plannerRoot = std::make_shared<AggPlan>(std::move(plannerRoot), query->agg_funcs,
                                                 query->agg_input_idxs, query->agg_is_star,
                                                 out_cols, group_idxs, query->group_by,
                                                 query->having);
        // 顶层Projection
        std::vector<TabCol> proj_cols;
        for (auto &cm : out_cols) {
            proj_cols.push_back({.tab_name = "", .col_name = cm.name});
        }
        plannerRoot = std::make_shared<ProjectionPlan>(T_Projection, std::move(plannerRoot),
                                                        std::move(proj_cols), false);
        // Reapply ORDER BY after aggregation
        if (sel_stmt && sel_stmt->has_sort) {
            TabCol sort_col;
            bool found_sort = false;
            for (auto& oc : out_cols) {
                if (oc.name == sel_stmt->order->cols->col_name) {
                    sort_col = {.tab_name = "", .col_name = oc.name};
                    found_sort = true;
                    break;
                }
            }
            if (found_sort) {
                plannerRoot = std::make_shared<SortPlan>(T_Sort, std::move(plannerRoot),
                                                          sort_col,
                                                          sel_stmt->order->orderby_dir == ast::OrderBy_DESC);
            }
        }
        return plannerRoot;
    }

    auto sel_cols = query->cols;
    auto original_conds = query->conds;
    bool is_star = query->cols_star_;
    if (!is_star) {
        plannerRoot = pushdown_projection_impl(plannerRoot, sel_cols, original_conds);
    }
    plannerRoot = std::make_shared<ProjectionPlan>(T_Projection, std::move(plannerRoot), 
                                                        std::move(sel_cols), is_star);

    return plannerRoot;
}

// 生成DDL语句和DML语句的查询执行计划
std::shared_ptr<Plan> Planner::do_planner(std::shared_ptr<Query> query, Context *context)
{
    std::shared_ptr<Plan> plannerRoot;
    if (auto x = std::dynamic_pointer_cast<ast::CreateTable>(query->parse)) {
        // create table;
        std::vector<ColDef> col_defs;
        for (auto &field : x->fields) {
            if (auto sv_col_def = std::dynamic_pointer_cast<ast::ColDef>(field)) {
                ColDef col_def = {.name = sv_col_def->col_name,
                                  .type = interp_sv_type(sv_col_def->type_len->type),
                                  .len = sv_col_def->type_len->len};
                col_defs.push_back(col_def);
            } else {
                throw InternalError("Unexpected field type");
            }
        }
        plannerRoot = std::make_shared<DDLPlan>(T_CreateTable, x->tab_name, std::vector<std::string>(), col_defs);
    } else if (auto x = std::dynamic_pointer_cast<ast::DropTable>(query->parse)) {
        // drop table;
        plannerRoot = std::make_shared<DDLPlan>(T_DropTable, x->tab_name, std::vector<std::string>(), std::vector<ColDef>());
    } else if (auto x = std::dynamic_pointer_cast<ast::CreateIndex>(query->parse)) {
        // create index;
        plannerRoot = std::make_shared<DDLPlan>(T_CreateIndex, x->tab_name, x->col_names, std::vector<ColDef>());
    } else if (auto x = std::dynamic_pointer_cast<ast::DropIndex>(query->parse)) {
        // drop index
        plannerRoot = std::make_shared<DDLPlan>(T_DropIndex, x->tab_name, x->col_names, std::vector<ColDef>());
    } else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(query->parse)) {
        // insert;
        plannerRoot = std::make_shared<DMLPlan>(T_Insert, std::shared_ptr<Plan>(),  x->tab_name,  
                                                    query->values, std::vector<Condition>(), std::vector<SetClause>());
    } else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(query->parse)) {
        // delete;
        // 生成表扫描方式
        std::shared_ptr<Plan> table_scan_executors;
        // 只有一张表，不需要进行物理优化了
        // int index_no = get_indexNo(x->tab_name, query->conds);
        std::vector<std::string> index_col_names;
        bool index_exist = get_index_cols(x->tab_name, query->conds, index_col_names);
        
        if (index_exist == false) {  // 该表没有索引
            index_col_names.clear();
            table_scan_executors = 
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, x->tab_name, query->conds, index_col_names);
        } else {  // 存在索引
            table_scan_executors =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, x->tab_name, query->conds, index_col_names);
        }

        plannerRoot = std::make_shared<DMLPlan>(T_Delete, table_scan_executors, x->tab_name,  
                                                std::vector<Value>(), query->conds, std::vector<SetClause>());
    } else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(query->parse)) {
        // update;
        // 生成表扫描方式
        std::shared_ptr<Plan> table_scan_executors;
        // 只有一张表，不需要进行物理优化了
        // int index_no = get_indexNo(x->tab_name, query->conds);
        std::vector<std::string> index_col_names;
        bool index_exist = get_index_cols(x->tab_name, query->conds, index_col_names);

        if (index_exist == false) {  // 该表没有索引
        index_col_names.clear();
            table_scan_executors = 
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, x->tab_name, query->conds, index_col_names);
        } else {  // 存在索引
            table_scan_executors =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, x->tab_name, query->conds, index_col_names);
        }
        plannerRoot = std::make_shared<DMLPlan>(T_Update, table_scan_executors, x->tab_name,
                                                     std::vector<Value>(), query->conds, 
                                                     query->set_clauses);
    } else if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse)) {

        std::shared_ptr<plannerInfo> root = std::make_shared<plannerInfo>(x);
        // 生成select语句的查询执行计划
        std::shared_ptr<Plan> projection = generate_select_plan(std::move(query), context);
        auto dml = std::make_shared<DMLPlan>(T_select, projection, std::string(), std::vector<Value>(),
                                                    std::vector<Condition>(), std::vector<SetClause>());
        // EXPLAIN标记和LIMIT通过Context传递
        context->explain_ = x->explain_analyze;
        context->limit_val_ = x->limit_val;
        plannerRoot = dml;
    } else {
        throw InternalError("Unexpected AST root");
    }
    return plannerRoot;
}