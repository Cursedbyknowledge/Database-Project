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

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "parser/ast.h"

#include "parser/parser.h"

typedef enum PlanTag{
    T_Invalid = 1,
    T_Help,
    T_ShowTable,
    T_ShowIndex,
    T_DescTable,
    T_CreateTable,
    T_DropTable,
    T_CreateIndex,
    T_DropIndex,
    T_SetKnob,
    T_Insert,
    T_Update,
    T_Delete,
    T_select,
    T_Transaction_begin,
    T_Transaction_commit,
    T_Transaction_abort,
    T_Transaction_rollback,
    T_SeqScan,
    T_IndexScan,
    T_NestLoop,
    T_SortMerge,    // sort merge join
    T_Sort,
    T_Projection
} PlanTag;

// 查询执行计划
class Plan
{
public:
    PlanTag tag;
    virtual ~Plan() = default;
    
    // EXPLAIN ANALYZE: 序列化计划树 + 运行时行数
    virtual void explain(int indent, const std::map<const Plan*, int>& rows_map,
                         std::string& out) const {}
};

class ScanPlan : public Plan
{
    public:
        ScanPlan(PlanTag tag, SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, std::vector<std::string> index_col_names)
        {
            Plan::tag = tag;
            tab_name_ = std::move(tab_name);
            conds_ = std::move(conds);
            TabMeta &tab = sm_manager->db_.get_table(tab_name_);
            cols_ = tab.cols;
            len_ = cols_.back().offset + cols_.back().len;
            fed_conds_ = conds_;
            index_col_names_ = index_col_names;
        
        }
        ~ScanPlan(){}
        void explain(int indent, const std::map<const Plan*, int>& rows_map,
                     std::string& out) const override {
            int rows = rows_map.count(this) ? rows_map.at(this) : 0;
            // 如果有过滤条件，先输出Filter节点
            if (!fed_conds_.empty()) {
                out += std::string(indent, '\t');
                out += "Filter(condition=[";
                for (size_t i = 0; i < fed_conds_.size(); i++) {
                    if(i) out += ", ";
                    auto &c = fed_conds_[i];
                    out += c.lhs_col.tab_name + "." + c.lhs_col.col_name;
                    // op
                    switch(c.op) {
                        case OP_EQ: out += "="; break;
                        case OP_NE: out += "<>"; break;
                        case OP_LT: out += "<"; break;
                        case OP_GT: out += ">"; break;
                        case OP_LE: out += "<="; break;
                        case OP_GE: out += ">="; break;
                    }
                    if (c.is_rhs_val) {
                        if (c.rhs_val.type == TYPE_INT)
                            out += std::to_string(*(int*)c.rhs_val.raw->data);
                        else if (c.rhs_val.type == TYPE_FLOAT)
                            out += std::to_string(*(float*)c.rhs_val.raw->data);
                        else out += c.rhs_val.str_val;
                    }
                }
                out += "], rows=" + std::to_string(rows) + ")\n";
                indent++;
            }
            out += std::string(indent, '\t');
            out += "Scan(table=" + tab_name_ + ", type=";
            out += (tag == T_IndexScan ? "IndexScan" : "SeqScan") + std::string(", rows=");
            out += std::to_string(rows) + ")\n";
        }
        // 以下变量同ScanExecutor中的变量
        std::string tab_name_;                     
        std::vector<ColMeta> cols_;                
        std::vector<Condition> conds_;             
        size_t len_;                               
        std::vector<Condition> fed_conds_;
        std::vector<std::string> index_col_names_;
    
};

class JoinPlan : public Plan
{
    public:
        JoinPlan(PlanTag tag, std::shared_ptr<Plan> left, std::shared_ptr<Plan> right, std::vector<Condition> conds)
        {
            Plan::tag = tag;
            left_ = std::move(left);
            right_ = std::move(right);
            conds_ = std::move(conds);
            type = INNER_JOIN;
        }
        ~JoinPlan(){}
        void explain(int indent, const std::map<const Plan*, int>& rows_map,
                     std::string& out) const override {
            int rows = rows_map.count(this) ? rows_map.at(this) : 0;
            out += std::string(indent, '\t');
            out += "Join(";
            // 递归收集表名（不用std::function，避免引入<functional>头文件）
            std::vector<std::string> tabs;
            collect_join_tables(left_, tabs);
            collect_join_tables(right_, tabs);
            out += "tables=[";
            for (size_t i = 0; i < tabs.size(); i++) { if(i) out += ", "; out += tabs[i]; }
            out += "], condition=[";
            for (size_t i = 0; i < conds_.size(); i++) {
                if(i) out += ", ";
                out += conds_[i].lhs_col.tab_name + "." + conds_[i].lhs_col.col_name;
                out += "=";
                out += conds_[i].rhs_col.tab_name + "." + conds_[i].rhs_col.col_name;
            }
            out += "], rows=" + std::to_string(rows) + ")\n";
            left_->explain(indent + 1, rows_map, out);
            right_->explain(indent + 1, rows_map, out);
        }
        // 递归收集Join树中的所有表名
        static void collect_join_tables(const std::shared_ptr<Plan>& p, std::vector<std::string>& tabs) {
            if (auto s = std::dynamic_pointer_cast<ScanPlan>(p)) {
                tabs.push_back(s->tab_name_);
            } else if (auto j = std::dynamic_pointer_cast<JoinPlan>(p)) {
                collect_join_tables(j->left_, tabs);
                collect_join_tables(j->right_, tabs);
            }
        }
        // 左节点
        std::shared_ptr<Plan> left_;
        // 右节点
        std::shared_ptr<Plan> right_;
        // 连接条件
        std::vector<Condition> conds_;
        // future TODO: 后续可以支持的连接类型
        JoinType type;
};

class ProjectionPlan : public Plan
{
    public:
        ProjectionPlan(PlanTag tag, std::shared_ptr<Plan> subplan, std::vector<TabCol> sel_cols)
        {
            Plan::tag = tag;
            subplan_ = std::move(subplan);
            sel_cols_ = std::move(sel_cols);
        }
        ~ProjectionPlan(){}
        void explain(int indent, const std::map<const Plan*, int>& rows_map,
                     std::string& out) const override {
            int rows = rows_map.count(this) ? rows_map.at(this) : 0;
            out += std::string(indent, '\t');
            out += "Project(columns=[";
            if (sel_cols_.empty() || (sel_cols_.size()==1 && sel_cols_[0].col_name=="*")) {
                out += "*";
            } else {
                for (size_t i = 0; i < sel_cols_.size(); i++) {
                    if(i) out += ", ";
                    out += sel_cols_[i].tab_name + "." + sel_cols_[i].col_name;
                }
            }
            out += "], rows=" + std::to_string(rows) + ")\n";
            subplan_->explain(indent + 1, rows_map, out);
        }
        std::shared_ptr<Plan> subplan_;
        std::vector<TabCol> sel_cols_;
        
};

class SortPlan : public Plan
{
    public:
        SortPlan(PlanTag tag, std::shared_ptr<Plan> subplan, TabCol sel_col, bool is_desc)
        {
            Plan::tag = tag;
            subplan_ = std::move(subplan);
            sel_col_ = sel_col;
            is_desc_ = is_desc;
        }
        ~SortPlan(){}
        std::shared_ptr<Plan> subplan_;
        TabCol sel_col_;
        bool is_desc_;
        
};

// dml语句，包括insert; delete; update; select语句　
class DMLPlan : public Plan
{
    public:
        DMLPlan(PlanTag tag, std::shared_ptr<Plan> subplan,std::string tab_name,
                std::vector<Value> values, std::vector<Condition> conds,
                std::vector<SetClause> set_clauses)
        {
            Plan::tag = tag;
            subplan_ = std::move(subplan);
            tab_name_ = std::move(tab_name);
            values_ = std::move(values);
            conds_ = std::move(conds);
            set_clauses_ = std::move(set_clauses);
        }
        ~DMLPlan(){}
        std::shared_ptr<Plan> subplan_;
        std::string tab_name_;
        std::vector<Value> values_;
        std::vector<Condition> conds_;
        std::vector<SetClause> set_clauses_;
};

// ddl语句, 包括create/drop table; create/drop index;
class DDLPlan : public Plan
{
    public:
        DDLPlan(PlanTag tag, std::string tab_name, std::vector<std::string> col_names, std::vector<ColDef> cols)
        {
            Plan::tag = tag;
            tab_name_ = std::move(tab_name);
            cols_ = std::move(cols);
            tab_col_names_ = std::move(col_names);
        }
        ~DDLPlan(){}
        std::string tab_name_;
        std::vector<std::string> tab_col_names_;
        std::vector<ColDef> cols_;
};

// help; show tables; desc tables; begin; abort; commit; rollback语句对应的plan
class OtherPlan : public Plan
{
    public:
        OtherPlan(PlanTag tag, std::string tab_name)
        {
            Plan::tag = tag;
            tab_name_ = std::move(tab_name);            
        }
        ~OtherPlan(){}
        std::string tab_name_;
};

// Set Knob Plan
class SetKnobPlan : public Plan
{
    public:
        SetKnobPlan(ast::SetKnobType knob_type, bool bool_value) {
            Plan::tag = T_SetKnob;
            set_knob_type_ = knob_type;
            bool_value_ = bool_value;
        }
    ast::SetKnobType set_knob_type_;
    bool bool_value_;
};

class plannerInfo{
    public:
    std::shared_ptr<ast::SelectStmt> parse;
    std::vector<Condition> where_conds;
    std::vector<TabCol> sel_cols;
    std::shared_ptr<Plan> plan;
    std::vector<std::shared_ptr<Plan>> table_scan_executors;
    std::vector<SetClause> set_clauses;
    plannerInfo(std::shared_ptr<ast::SelectStmt> parse_):parse(std::move(parse_)){}

};
