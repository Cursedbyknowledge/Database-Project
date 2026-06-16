---
name: topic4-explain-format-fix
overview: 修复EXPLAIN ANALYZE输出中3个格式问题：别名保存与逆向查找、FLOAT值去尾零、顶部Project列名
todos:
  - id: add-rev-alias-map
    content: 在 Query 和 Context 中添加 rev_alias_map_(real→alias)，analyze.cpp 构建反向映射，planner.cpp 写入 context
    status: completed
  - id: fix-alias-in-explain
    content: 在 explain_plan 中所有 tab_name 输出处查找别名替代，Scan的tab_name_、Filter列名、Join条件、Project列名、Join tables
    status: completed
    dependencies:
      - add-rev-alias-map
  - id: fix-float-format
    content: 在 explain_plan 中修复 FLOAT 格式化：自定义 format_float 函数去除尾随零
    status: completed
  - id: verify-and-push
    content: 编译验证、运行 test_topic4.py 确认 25/25、提交推送两个仓库
    status: completed
    dependencies:
      - fix-alias-in-explain
      - fix-float-format
---

## 用户需求

修复CI中8个basic_query_test全部 `answer mismatch` 的问题。本地25/25测试通过但因断言过于宽松（子串匹配），未能检测到EXPLAIN ANALYZE输出格式问题。

## 核心缺陷（对照测试说明文档2026.pdf第8-11页确认）

### 缺陷1：表别名丢失

赛题明确要求"如果表有别名，则使用别名"。但 `analyze.cpp` 将别名 `c→customers` 解析后丢弃了原始别名信息，`explain_plan` 只能从已解析的完整表名输出。

- CI期望：`condition=[c.customer_id=o.customer_id]`
- 当前输出：`condition=[customers.customer_id=orders.customer_id]`

### 缺陷2：FLOAT值尾随零

- CI期望：`o.total_amount>1000`
- 当前输出：`1000.000000`（std::to_string保留6位小数）

### 缺陷3：顶部Project列名使用全名而非别名

- CI期望：`Project(columns=[c.name, o.order_id], rows=5)`
- 当前输出：列名前缀为全名（customers.xxx, orders.xxx）

## 技术方案

### 实施策略

构建 reverse alias map（real_table_name → alias_name），通过 Context 传递给 explain_plan，在输出时查找显示别名。修复 FLOAT 格式化逻辑。最小化侵入，不修改 Plan 类布局。

### 修改文件清单

| 文件 | 修改内容 | 类型 |
| --- | --- | --- |
| `src/analyze/analyze.h` | Query 添加 `rev_alias_map_` | MODIFY |
| `src/analyze/analyze.cpp` | 构建 reverse alias map | MODIFY |
| `src/common/context.h` | Context 添加 `rev_alias_map_` | MODIFY |
| `src/optimizer/planner.cpp` | do_planner将alias_map写入context | MODIFY |
| `src/execution/execution_manager.cpp` | explain_plan使用别名+修复FLOAT格式 | MODIFY |


### 数据流

```
yacc: alias_map(alias→real) = {c→customers, o→orders}
  ↓
analyze.cpp: rev_alias_map(real→alias) = {customers→c, orders→o}
  ↓ store in Query.rev_alias_map_
planner.cpp: do_planner → context.rev_alias_map_ = query.rev_alias_map_
  ↓
execution_manager.cpp: explain_plan:
  读取context.rev_alias_map_ → 查找所有 tab_name → 用别名输出
  FLOAT: 自定义格式函数去除尾随零
```

### 核心实现逻辑

1. **analyze.cpp 构建 reverse map**：在 alias 解析完成后，`for(auto &[alias, real] : x->alias_map) query->rev_alias_map_[real] = alias;`

2. **planner.cpp 传递**：`if(x->explain_analyze) context->rev_alias_map_ = ...`

3. **explain_plan 查找别名**：每处 tab_name 输出前查找 `auto display = alias_map.count(tab_name) ? alias_map.at(tab_name) : tab_name;`

4. **FLOAT 格式化**：用 `std::ostringstream` + `std::fixed` + 去除尾零逻辑替代 `std::to_string`