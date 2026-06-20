%{
#include "ast.h"
#include "yacc.tab.h"
#include <iostream>
#include <memory>
#include <map>

int yylex(YYSTYPE *yylval, YYLTYPE *yylloc);

void yyerror(YYLTYPE *locp, const char* s) {
    std::cerr << "Parser Error at line " << locp->first_line << " column " << locp->first_column << ": " << s << std::endl;
}

using namespace ast;
// 全局变量：暂存JOIN ON条件和别名映射，供SELECT规则合并
static std::vector<std::shared_ptr<BinaryExpr>> g_join_on_conds;
static std::map<std::string, std::string> g_alias_map_;
%}

%define api.pure full
%locations
%define parse.error verbose

// keywords
%token SHOW TABLES CREATE TABLE DROP DESC INSERT INTO VALUES DELETE FROM ASC ORDER BY
WHERE UPDATE SET SELECT INT CHAR FLOAT INDEX AND JOIN EXIT HELP
TXN_BEGIN TXN_COMMIT TXN_ABORT TXN_ROLLBACK ORDER_BY
ENABLE_NESTLOOP ENABLE_SORTMERGE
EXPLAIN ANALYZE COUNT MAX_TOKEN MIN_TOKEN SUM_TOKEN AVG
GROUP HAVING LIMIT UNION_TOKEN ALL ON AS
ISOLATION LEVEL SNAPSHOT_TOKEN SERIALIZABLE TRANSACTION
CHECKPOINT STATIC_CHECKPOINT

// non-keywords
%token LEQ NEQ GEQ T_EOF

// type-specific tokens
%token <sv_str> IDENTIFIER VALUE_STRING
%token <sv_int> VALUE_INT
%token <sv_float> VALUE_FLOAT
%token <sv_bool> VALUE_BOOL

// specify types for non-terminal symbol
%type <sv_node> stmt dbStmt ddl dml txnStmt setStmt
%type <sv_field> field
%type <sv_fields> fieldList
%type <sv_type_len> type
%type <sv_comp_op> op
%type <sv_expr> expr
%type <sv_val> value
%type <sv_vals> valueList
%type <sv_str> tbName colName
%type <sv_strs> tableList colNameList opt_group_clause
%type <sv_col> col
%type <sv_col> having_agg
%type <sv_cols> colList selector
%type <sv_set_clause> setClause
%type <sv_set_clauses> setClauses
%type <sv_cond> condition
%type <sv_cond> having_condition
%type <sv_col> having_lhs
%type <sv_conds> whereClause optWhereClause havingClause optHavingClause
%type <sv_orderby> order_clause opt_order_clause
%type <sv_orderby_dir> opt_asc_desc
%type <sv_setKnobType> set_knob_type
%type <sv_cols> agg_item
%type <sv_int> opt_limit
%type <sv_node> union_select
%type <sv_sub_selects> union_list

%%
start:
        stmt ';'
    {
        parse_tree = $1;
        YYACCEPT;
    }
    |   HELP
    {
        parse_tree = std::make_shared<Help>();
        YYACCEPT;
    }
    |   EXIT
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
    |   T_EOF
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
    ;

stmt:
        dbStmt
    |   ddl
    |   dml
    |   txnStmt
    |   setStmt
    ;

txnStmt:
        TXN_BEGIN
    {
        $$ = std::make_shared<TxnBegin>();
    }
    |   TXN_COMMIT
    {
        $$ = std::make_shared<TxnCommit>();
    }
    |   TXN_ABORT
    {
        $$ = std::make_shared<TxnAbort>();
    }
    |   TXN_ROLLBACK
    {
        $$ = std::make_shared<TxnRollback>();
    }
    ;

dbStmt:
        SHOW TABLES
    {
        $$ = std::make_shared<ShowTables>();
    }
    |   SHOW INDEX FROM tbName
    {
        $$ = std::make_shared<ShowIndex>($4);
    }
    ;

setStmt:
        SET set_knob_type '=' VALUE_BOOL
    {
        $$ = std::make_shared<SetStmt>($2, $4);
    }
    |   SET TRANSACTION ISOLATION LEVEL SNAPSHOT_TOKEN ISOLATION
    {
        $$ = std::make_shared<SetIsolationLevel>("SNAPSHOT_ISOLATION");
    }
    |   SET TRANSACTION ISOLATION LEVEL SERIALIZABLE
    {
        $$ = std::make_shared<SetIsolationLevel>("SERIALIZABLE");
    }
    ;

ddl:
        CREATE TABLE tbName '(' fieldList ')'
    {
        $$ = std::make_shared<CreateTable>($3, $5);
    }
    |   DROP TABLE tbName
    {
        $$ = std::make_shared<DropTable>($3);
    }
    |   DESC tbName
    {
        $$ = std::make_shared<DescTable>($2);
    }
    |   CREATE INDEX tbName '(' colNameList ')'
    {
        $$ = std::make_shared<CreateIndex>($3, $5);
    }
    |   DROP INDEX tbName '(' colNameList ')'
    {
        $$ = std::make_shared<DropIndex>($3, $5);
    }
    |   CREATE STATIC_CHECKPOINT
    {
        $$ = std::make_shared<CreateStaticCheckpoint>();
    }
    ;

dml:
        INSERT INTO tbName VALUES '(' valueList ')'
    {
        $$ = std::make_shared<InsertStmt>($3, $6);
    }
    |   DELETE FROM tbName optWhereClause
    {
        $$ = std::make_shared<DeleteStmt>($3, $4);
    }
    |   UPDATE tbName SET setClauses optWhereClause
    {
        $$ = std::make_shared<UpdateStmt>($2, $4, $5);
    }
    |   SELECT selector FROM tableList optWhereClause opt_group_clause optHavingClause opt_order_clause opt_limit
    {
        auto merged_conds = $5;
        merged_conds.insert(merged_conds.end(), g_join_on_conds.begin(), g_join_on_conds.end());
        g_join_on_conds.clear();
        auto sel = std::make_shared<SelectStmt>($2, $4, merged_conds, $8);
        sel->group_by = $6;
        sel->having = $7;
        if ($9 > 0) sel->limit_val = $9;
        sel->alias_map = g_alias_map_; g_alias_map_.clear();
        $$ = sel;
    }
    |   EXPLAIN ANALYZE SELECT selector FROM tableList optWhereClause opt_order_clause
    {
        auto merged_conds = $7;
        merged_conds.insert(merged_conds.end(), g_join_on_conds.begin(), g_join_on_conds.end());
        g_join_on_conds.clear();
        auto sel = std::make_shared<SelectStmt>($4, $6, merged_conds, $8);
        sel->explain_analyze = true;
        sel->alias_map = g_alias_map_; g_alias_map_.clear();
        $$ = sel;
    }
    |   SELECT selector FROM '(' union_list ')' AS IDENTIFIER opt_order_clause
    {
        $$ = std::make_shared<UnionStmt>($5, $8, $9);
    }
    |   EXPLAIN ANALYZE SELECT selector FROM '(' union_list ')' AS IDENTIFIER opt_order_clause
    {
        auto us = std::make_shared<UnionStmt>($7, $10, $11);
        us->explain_analyze = true;
        $$ = us;
    }
    ;

union_select:
    SELECT selector FROM tableList optWhereClause
    {
        auto merged_conds = $5;
        merged_conds.insert(merged_conds.end(), g_join_on_conds.begin(), g_join_on_conds.end());
        g_join_on_conds.clear();
        auto sel = std::make_shared<SelectStmt>($2, $4, merged_conds, nullptr);
        sel->alias_map = g_alias_map_; g_alias_map_.clear();
        $$ = sel;
    }
    ;

union_list:
    union_select
    {
        $$ = std::vector<std::shared_ptr<SelectStmt>>{std::dynamic_pointer_cast<SelectStmt>($1)};
    }
    |   union_list UNION_TOKEN union_select
    {
        $$.push_back(std::dynamic_pointer_cast<SelectStmt>($3));
    }
    ;

opt_group_clause:
        /* epsilon */ { $$ = std::vector<std::string>(); }
    |   GROUP BY colNameList { $$ = $3; }
    ;

optHavingClause:
        /* epsilon */ { /* ignore */ }
    |   HAVING havingClause
    {
        $$ = $2;
    }
    ;

opt_limit:
        /* epsilon */ { $$ = -1; }
    |   LIMIT VALUE_INT
    {
        $$ = $2;
    }
    ;

fieldList:
        field
    {
        $$ = std::vector<std::shared_ptr<Field>>{$1};
    }
    |   fieldList ',' field
    {
        $$.push_back($3);
    }
    ;

colNameList:
        colName
    {
        $$ = std::vector<std::string>{$1};
    }
    | colNameList ',' colName
    {
        $$.push_back($3);
    }
    ;

field:
        colName type
    {
        $$ = std::make_shared<ColDef>($1, $2);
    }
    ;

type:
        INT
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_INT, sizeof(int));
    }
    |   CHAR '(' VALUE_INT ')'
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_STRING, $3);
    }
    |   FLOAT
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_FLOAT, sizeof(float));
    }
    ;

valueList:
        value
    {
        $$ = std::vector<std::shared_ptr<Value>>{$1};
    }
    |   valueList ',' value
    {
        $$.push_back($3);
    }
    ;

value:
        VALUE_INT
    {
        $$ = std::make_shared<IntLit>($1);
    }
    |   VALUE_FLOAT
    {
        $$ = std::make_shared<FloatLit>($1);
    }
    |   VALUE_STRING
    {
        $$ = std::make_shared<StringLit>($1);
    }
    |   VALUE_BOOL
    {
        $$ = std::make_shared<BoolLit>($1);
    }
    ;

condition:
        col op expr
    {
        $$ = std::make_shared<BinaryExpr>($1, $2, $3);
    }
    ;

optWhereClause:
        /* epsilon */ { /* ignore*/ }
    |   WHERE whereClause
    {
        $$ = $2;
    }
    ;

whereClause:
        condition 
    {
        $$ = std::vector<std::shared_ptr<BinaryExpr>>{$1};
    }
    |   whereClause AND condition
    {
        $$.push_back($3);
    }
    ;

havingClause:
        having_condition
    {
        $$ = std::vector<std::shared_ptr<BinaryExpr>>{$1};
    }
    |   havingClause AND having_condition
    {
        $$.push_back($3);
    }
    ;

having_condition:
        having_lhs op value
    {
        $$ = std::make_shared<BinaryExpr>($1, $2, $3);
    }
    ;

having_lhs:
        col
    {
        $$ = $1;
    }
    |   having_agg
    {
        $$ = $1;
    }
    ;

having_agg:
        COUNT '(' '*' ')'
    {
        auto c = std::make_shared<Col>("*", "*");
        c->is_agg = true;
        c->agg_func = "COUNT";
        $$ = c;
    }
    |   COUNT '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "COUNT";
        $$ = c;
    }
    |   MAX_TOKEN '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "MAX";
        $$ = c;
    }
    |   MIN_TOKEN '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "MIN";
        $$ = c;
    }
    |   SUM_TOKEN '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "SUM";
        $$ = c;
    }
    |   AVG '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "AVG";
        $$ = c;
    }
    ;
col:
        tbName '.' colName
    {
        $$ = std::make_shared<Col>($1, $3);
    }
    |   colName
    {
        $$ = std::make_shared<Col>("", $1);
    }
    ;

colList:
        col
    {
        $$ = std::vector<std::shared_ptr<Col>>{$1};
    }
    |   col AS colName
    {
        auto c = $1; c->col_name = $3;
        $$ = std::vector<std::shared_ptr<Col>>{c};
    }
    |   agg_item
    {
        $$ = $1;
    }
    |   agg_item AS colName
    {
        auto v = $1;
        for (auto &c : v) { if (c->tab_name.empty()) c->tab_name = c->col_name; c->col_name = $3; }
        $$ = v;
    }
    |   colList ',' col
    {
        $$.push_back($3);
    }
    |   colList ',' col AS colName
    {
        auto c = $3; c->col_name = $5; $$.push_back(c);
    }
    |   colList ',' agg_item
    {
        for (auto &c : $3) $$.push_back(c);
    }
    |   colList ',' agg_item AS colName
    {
        for (auto &c : $3) { if (c->tab_name.empty()) c->tab_name = c->col_name; c->col_name = $5; $$.push_back(c); }
    }
    ;

op:
        '='
    {
        $$ = SV_OP_EQ;
    }
    |   '<'
    {
        $$ = SV_OP_LT;
    }
    |   '>'
    {
        $$ = SV_OP_GT;
    }
    |   NEQ
    {
        $$ = SV_OP_NE;
    }
    |   LEQ
    {
        $$ = SV_OP_LE;
    }
    |   GEQ
    {
        $$ = SV_OP_GE;
    }
    ;

expr:
        value
    {
        $$ = std::static_pointer_cast<Expr>($1);
    }
    |   col
    {
        $$ = std::static_pointer_cast<Expr>($1);
    }
    ;

setClauses:
        setClause
    {
        $$ = std::vector<std::shared_ptr<SetClause>>{$1};
    }
    |   setClauses ',' setClause
    {
        $$.push_back($3);
    }
    ;

setClause:
        colName '=' value
    {
        $$ = std::make_shared<SetClause>($1, $3);
    }
    ;

selector:
        '*'
    {
        $$ = {};
    }
    |   colList
    ;

    // agg_selector 已合并到 colList，不再需要独立规则

agg_item:
        COUNT '(' '*' ')'
    {
        auto c = std::make_shared<Col>("", "*");
        c->is_agg = true;
        c->agg_func = "COUNT";
        $$ = std::vector<std::shared_ptr<Col>>{c};
    }
    |   COUNT '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "COUNT";
        $$ = std::vector<std::shared_ptr<Col>>{c};
    }
    |   MAX_TOKEN '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "MAX";
        $$ = std::vector<std::shared_ptr<Col>>{c};
    }
    |   MIN_TOKEN '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "MIN";
        $$ = std::vector<std::shared_ptr<Col>>{c};
    }
    |   SUM_TOKEN '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "SUM";
        $$ = std::vector<std::shared_ptr<Col>>{c};
    }
    |   AVG '(' colName ')'
    {
        auto c = std::make_shared<Col>("", $3);
        c->is_agg = true;
        c->agg_func = "AVG";
        $$ = std::vector<std::shared_ptr<Col>>{c};
    }
    ;

tableList:
        tbName
    {
        $$ = std::vector<std::string>{$1};
    }
    |   tbName IDENTIFIER
    {
        $$ = std::vector<std::string>{$1};  // tabs存真实表名
        g_alias_map_[$2] = $1;
    }
    |   tableList ',' tbName
    {
        $$.push_back($3);
    }
    |   tableList ',' tbName IDENTIFIER
    {
        $$.push_back($3);  // tabs存真实表名
        g_alias_map_[$4] = $3;
    }
    |   tableList JOIN tbName
    {
        $$.push_back($3);
    }
    |   tableList JOIN tbName IDENTIFIER
    {
        $$.push_back($3);  // tabs存真实表名
        g_alias_map_[$4] = $3;
    }
    |   tableList JOIN tbName ON whereClause
    {
        $$.push_back($3);
        g_join_on_conds.insert(g_join_on_conds.end(), $5.begin(), $5.end());
    }
    |   tableList JOIN tbName IDENTIFIER ON whereClause
    {
        $$.push_back($3);  // tabs存真实表名
        g_alias_map_[$4] = $3;
        g_join_on_conds.insert(g_join_on_conds.end(), $6.begin(), $6.end());
    }
    ;

opt_order_clause:
    ORDER BY order_clause      
    { 
        $$ = $3; 
    }
    |   /* epsilon */ { /* ignore*/ }
    ;

order_clause:
      col  opt_asc_desc
    {
        std::vector<std::shared_ptr<Col>> cols = {$1};
        std::vector<OrderByDir> dirs = {$2};
        $$ = std::make_shared<OrderBy>(cols, dirs);
    }
    | order_clause ',' col opt_asc_desc
    {
        $1->cols.push_back($3);
        $1->orderby_dirs.push_back($4);
        $$ = $1;
    }
    ;

opt_asc_desc:
    ASC          { $$ = OrderBy_ASC;     }
    |  DESC      { $$ = OrderBy_DESC;    }
    |       { $$ = OrderBy_DEFAULT; }
    ;    

set_knob_type:
    ENABLE_NESTLOOP { $$ = EnableNestLoop; }
    |   ENABLE_SORTMERGE { $$ = EnableSortMerge; }
    ;

tbName: IDENTIFIER;

colName: IDENTIFIER;
%%
