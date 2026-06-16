---
name: fix-select-star-explains
overview: 修复 SELECT * 在 EXPLAIN ANALYZE 输出中未显示为 columns=[*] 的问题，通过添加标记位将原始星号信息从 analyze 阶段传递到 explain_plan。
todos:
  - id: add-query-star-flag
    content: 在 analyze.h 的 Query 类中添加 cols_star_ 标记，analyze.cpp 中检测到 SELECT * 时设置标记
    status: completed
  - id: add-projectionplan-star-flag
    content: 在 plan.h 的 ProjectionPlan 类中添加 is_star_ 字段和构造函数参数
    status: completed
  - id: planner-pass-star-flag
    content: 修改 planner.cpp 的 generate_select_plan，使用 query->cols_star_ 替代原有检测，传递给 ProjectionPlan
    status: completed
    dependencies:
      - add-query-star-flag
      - add-projectionplan-star-flag
  - id: explain-check-star-flag
    content: 修改 execution_manager.cpp 的 explain_plan，检查 pp->is_star_ 输出 columns=[*]
    status: completed
    dependencies:
      - add-projectionplan-star-flag
  - id: build-and-test
    content: 编译构建，运行 test_topic4.py 验证本地测试通过，确认 output.txt 格式正确
    status: completed
    dependencies:
      - planner-pass-star-flag
      - explain-check-star-flag
---

## 产品概述

修复 `EXPLAIN ANALYZE SELECT *` 查询中 Project 节点输出 `columns=[*]` 而非完整列名列表的问题，使 CI basic_query_test2~8 的 "answer mismatch standard answer" 错误得以解决。

## 核心功能

- **SELECT * 标记传递链**：从 `analyze.cpp`（识别 SELECT *）→ `Query.cols_star_` → `planner.cpp`（传递标记）→ `ProjectionPlan.is_star_` → `explain_plan`（输出 `[*]`），形成一个完整的标记传递链
- **保持列展开**：`sel_cols_` 仍然包含展开后的完整列名列表，确保 `ProjectionExecutor`、`select_from` 结果展示等在执行器层面正常工作
- **EXPLAIN 输出符合赛题规范**：只修改 `explain_plan` 的输出判断，使其在遇到 SELECT  *时输出 `columns=[*]` 而非完整列名列表

## 技术方案

### 实现策略

通过添加 `is_star` 布尔标记，在不改变 `sel_cols_` 数据内容的前提下，让 `explain_plan` 能区分 SELECT * 和显式列名查询，从而输出正确的格式。

### 数据流

```
parser (yacc.y:437-439)
  → SELECT * 时 x->cols = {} 
  → analyze.cpp: 检测 cols 为空, 设置 query->cols_star_ = true, 同时展开所有列到 query->cols
  → planner.cpp: 读取 query->cols_star_, 创建 ProjectionPlan 时传入 is_star_ 标记
  → explain_plan: 检查 pp->is_star_, 若为 true 则输出 "columns=[*]"
```

### 修改详情

#### 1. src/analyze/analyze.h — Query 类添加标记

在第 31 行 `std::vector<TabCol> cols;` 之后新增：

```cpp
bool cols_star_ = false;  // true if original SQL was SELECT *
```

#### 2. src/analyze/analyze.cpp — 设置标记

在第 58 行 `if (query->cols.empty())` 块内，列展开前添加：

```cpp
query->cols_star_ = true;  // 标记原始查询为 SELECT *
```

#### 3. src/optimizer/plan.h — ProjectionPlan 添加标记

修改构造函数签名和类成员：

```cpp
class ProjectionPlan : public Plan {
public:
    ProjectionPlan(PlanTag tag, std::shared_ptr<Plan> subplan, 
                   std::vector<TabCol> sel_cols, bool is_star = false) {
        Plan::tag = tag;
        subplan_ = std::move(subplan);
        sel_cols_ = std::move(sel_cols);
        is_star_ = is_star;
    }
    std::shared_ptr<Plan> subplan_;
    std::vector<TabCol> sel_cols_;
    bool is_star_ = false;
};
```

注意：第 426 行 `pushdown_projection_impl` 中创建子 Project 节点时不传 `is_star` 参数（使用默认值 false）。

#### 4. src/optimizer/planner.cpp — generate_select_plan

- 第 442 行：将 `bool is_star = (sel_cols.size() == 1 && sel_cols[0].col_name == "*");` 改为 `bool is_star = query->cols_star_;`
- 第 446-447 行：构造 ProjectionPlan 时传入 `is_star` 标记：

```cpp
plannerRoot = std::make_shared<ProjectionPlan>(T_Projection, std::move(plannerRoot),
std::move(sel_cols), is_star);
```

#### 5. src/execution/execution_manager.cpp — explain_plan

第 277 行条件由：

```cpp
if (pp->sel_cols_.empty() || (pp->sel_cols_.size() == 1 && pp->sel_cols_[0].col_name == "*")) {
```

改为：

```cpp
if (pp->is_star_ || pp->sel_cols_.empty() || (pp->sel_cols_.size() == 1 && pp->sel_cols_[0].col_name == "*")) {
```

### 关键设计决策

- **为什么不直接改变 sel_cols_ 内容**：ProjectionExecutor 和 select_from 依赖 sel_cols_ 中的真实列名来构建投影和执行结果输出。将 `*` 放入 sel_cols_ 会导致执行器级崩溃
- **为什么使用标记而非在 explain_plan 中重新检测**：在 explain_plan 层面重新检测"是否所有列"是脆弱且低效的，需要一个额外的表结构查询。标记传递更简洁可靠
- **sub-Project 不受影响**：pushdown_projection_impl 创建的子 Project 节点有明确的列列表，is_star_ 为默认值 false，不影响其输出