---
name: topic4-remaining-fixes
overview: 修复题目四剩余的3个CI不匹配问题：Join condition丢失、投影下推未实现、EXPLAIN同层节点排序
todos:
  - id: fix-join-conds
    content: 修复 planner.cpp 中 JoinPlan 包装时的空条件问题：合并左右子 JoinPlan 的 conds 到外层
    status: completed
  - id: add-projection-pushdown
    content: 在 planner.cpp 的 generate_select_plan 中添加投影下推 pushdown_projection() 辅助函数，递归为 Join 下方每表插入 Project 节点
    status: completed
    dependencies:
      - fix-join-conds
  - id: sort-join-conditions
    content: 在 execution_manager.cpp 的 explain_plan 中对 Join 的 conds_ 按字典序排序后输出
    status: completed
  - id: build-and-test
    content: 构建、运行 test_topic4.py 验证 18/18 通过，提交推送两个仓库
    status: completed
    dependencies:
      - fix-join-conds
      - add-projection-pushdown
      - sort-join-conditions
---

## 用户需求

CI 全部 8 个 basic_query_test 报 "answer mismatch standard answer"。本地 18/18 测试全通过但与 CI 期望格式不匹配。需实现赛题要求的三项核心修复。

## 产品概述

题目四要求实现 EXPLAIN ANALYZE + 谓词下推 + 投影下推。当前谓词下推和行计数已完成（本地 18/18），但以下三项导致 CI mismatch。

## 核心缺陷

### 缺陷1：Join condition=[] — JoinPlan 丢失连接条件

`planner.cpp` 第304-306行，两表 JOIN 的第二层包装 JoinPlan 用空 `std::vector<Condition>()` 创建，丢失了内层 JoinPlan 中已有的 `c.customer_id=o.customer_id` 条件。CI 期望 `condition=[c.customer_id=o.customer_id]`。

### 缺陷2：投影下推未实现 — 缺少 Project below Join

planner 仅做了谓词下推，未做投影下推。当 SELECT 指定列而非 SELECT * 时（如 `SELECT c.name, o.order_id`），需要在 Join 下方为每个表插入 Project 节点。CI 期望：

```
Join(...)
    Project(columns=[c.customer_id, c.name], rows=3)
        Scan(table=customers, ...)
    Project(columns=[o.customer_id, o.order_id], rows=15)
        Scan(table=orders, ...)
```

### 缺陷3：Join condition 条件未按字典序排序

`explain_plan` 中 Join 的 condition 输出时未排序。赛题规范要求连接条件按字典序输出。

## 技术栈

- C++17（项目现有）
- 修改文件：`src/optimizer/planner.cpp`、`src/execution/execution_manager.cpp`

## 实现方案

### 修复1：Join condition=[] — 合并子 JoinPlan 条件到根

**根因**：`planner.cpp` 第304-306行 `make_one_rel` 中，第一个 join loop 创建 `JoinPlan(left=ScanPlan, right=ScanPlan, conds={join_cond})` 后，第二个 loop 用空条件创建外层 JoinPlan 包装。

**修复**：在包装 JoinPlan 时，从左右子 JoinPlan 收集 conds 合并到外层。修改第304-306行和类似位置：

```cpp
// 旧代码：空条件
table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, 
    std::move(temp_join_executors), std::move(table_join_executors), 
    std::vector<Condition>());

// 新代码：合并子 JoinPlan 的 conds
auto left_conds = std::dynamic_pointer_cast<JoinPlan>(temp_join_executors)->conds_;
auto right_conds = std::dynamic_pointer_cast<JoinPlan>(table_join_executors)->conds_;
left_conds.insert(left_conds.end(), right_conds.begin(), right_conds.end());
table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, 
    std::move(temp_join_executors), std::move(table_join_executors), 
    std::move(left_conds));
```

### 修复2：投影下推

**策略**：在 `generate_select_plan` 返回 ProjectionPlan 前，遍历 sel_cols，按表分组，递归下推 Project 节点。

**实现**：

1. 添加辅助函数 `pushdown_projection(std::shared_ptr<Plan> plan, const std::vector<TabCol>& sel_cols)`
2. 对 JoinPlan 的左右子树分别插入 Project 节点（仅保留该表在 sel_cols 中需要的列 + join 条件需要的列）
3. 对 ScanPlan 直接替换为 ProjectionPlan(ScanPlan, filtered_cols)
4. sel_cols 为空或含 "*" 时跳过（不插入 Project）

**关键逻辑**：

- 从 sel_cols 中筛选属于该表的列
- 从 join conds 中提取该表参与连接的列（必须保留）
- 投影下推后，列可能只包含少数列，不会丢失数据因为执行器仍扫描完整行

### 修复3：Join condition 字典序排序

在 `explain_plan` 的 Join 输出块中，对 `jp->conds_` 进行排序后再输出：

```cpp
auto sorted_conds = jp->conds_;
std::sort(sorted_conds.begin(), sorted_conds.end(), [](auto &a, auto &b) {
    return (a.lhs_col.tab_name + "." + a.lhs_col.col_name) <
           (b.lhs_col.tab_name + "." + b.lhs_col.col_name);
});
```

## 目录结构

```
project-root/
├── src/
│   ├── optimizer/
│   │   └── planner.cpp          # [MODIFY] 修复Join condition合并 + 新增投影下推pushdown_projection()
│   └── execution/
│       └── execution_manager.cpp # [MODIFY] Join condition字典序排序输出
```

## 架构设计

```mermaid
graph TD
    A[SELECT sel_cols FROM t1 JOIN t2 ON join_cond WHERE filter_cond] --> B[Parser: AST]
    B --> C[Analyze: Query{cols, tabs, conds}]
    C --> D[logical_optimization: 去重]
    D --> E[make_one_rel: 谓词下推 + Join生成]
    E --> F[pushdown_projection: 投影下推 NEW]
    F --> G[ProjectionPlan包装根节点]
    G --> H[Portal: Executor树]
    H --> I[explain_select: 执行 + 收集行数]
    I --> J[explain_plan: 序列化 + 排序输出]
```