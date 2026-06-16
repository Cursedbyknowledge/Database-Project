---
name: fix-basic-query-ci-failures
overview: 修复 CI 中 basic_query_test2/5/6/7/8 的 EXPLAIN ANALYZE 输出不匹配问题。诊断选择下推、投影下推、连接条件输出的格式问题。
todos:
  - id: fix-indexscan-runtime-output
    content: 在 executor_index_scan.h 的 Next() 中，过滤检查通过后添加 runtime_output_++
    status: completed
  - id: build-and-test
    content: 编译构建并运行 test_topic4.py 和单元测试验证无回归
    status: completed
    dependencies:
      - fix-indexscan-runtime-output
  - id: commit-and-push
    content: 提交代码并推送到 eduxiji 仓库让 CI 重新评分
    status: completed
    dependencies:
      - build-and-test
---

## 用户需求

CI 评分 3.30，basic_query_test2/5/6/7/8 五个测试因 "answer mismatch standard answer" 失败。需修复 EXPLAIN ANALYZE 输出与 CI 期望不一致的问题。

## 根因定位

**IndexScanExecutor 缺少 runtime_output_ 计数**：`src/execution/executor_index_scan.h:118-141` 的 `Next()` 方法中仅递增 `runtime_rows_++`（第 120 行），过滤通过后从未递增 `runtime_output_++`。对比 `SeqScanExecutor`（executor_seq_scan.h:60-67）正确地在过滤通过后递增 `runtime_output_++`。

CI 中 tests 5-8 很可能涉及 CREATE INDEX 后走 IndexScan 路径的查询，导致 EXPLAIN 输出中 Filter 节点的 `rows=0` 而非正确的过滤行数。

## 修复内容

- 在 `IndexScanExecutor::Next()` 中，过滤条件全部通过的位置添加 `runtime_output_++`，使其与 `SeqScanExecutor` 行为一致
- 构建并运行现有测试脚本验证修复不影响既有功能
- 提交代码让 CI 重新评分

## 修改方案

### 修复 IndexScanExecutor::Next() — 添加 runtime_output_++

**文件**：`src/execution/executor_index_scan.h`

**当前代码**（第 118-141 行）：

```cpp
std::unique_ptr<RmRecord> Next() override {
    if (scan_->is_end()) return nullptr;
    runtime_rows_++;
    rid_ = scan_->rid();
    try {
        auto rec = fh_->get_record(rid_, context_);
        if (rec != nullptr) {
            for (auto& cond : fed_conds_) {
                if (!eval_cond(rec->data, cond, cols_)) {
                    return nullptr;  // 过滤掉，runtime_output_ 不变
                }
            }
            return rec;  // 通过过滤，但 runtime_output_ 未递增 ❌
        }
    } catch (...) { ... }
    return nullptr;
}
```

**修复后**：

```cpp
std::unique_ptr<RmRecord> Next() override {
    if (scan_->is_end()) return nullptr;
    runtime_rows_++;
    rid_ = scan_->rid();
    try {
        auto rec = fh_->get_record(rid_, context_);
        if (rec != nullptr) {
            for (auto& cond : fed_conds_) {
                if (!eval_cond(rec->data, cond, cols_)) {
                    return nullptr;
                }
            }
            runtime_output_++;  // ✅ 过滤通过后递增
            return rec;
        }
    } catch (...) { ... }
    return nullptr;
}
```

### 数据流验证

修复后 EXPLAIN ANALYZE 数据收集链路：

```
explain_select → execute executor tree (runtime_rows_/runtime_output_ 递增)
  → collect(plan, executor_root): rows_map = runtime_rows_, out_rows_map = runtime_output_
    → explain_plan: Filter 行显示 out_rows_map，Scan 行显示 rows_map
```

对于 IndexScan + Filter 场景：

- Filter rows = `runtime_output_` = 正确过滤行数（修复后）
- Scan rows = `runtime_rows_` = 总扫描行数

### 执行说明

- 仅修改一行代码，影响范围明确
- SeqScanExecutor 已有正确的 runtime_output_++ 作为参考
- IndexScan 和 SeqScan 共享相同的 explain_plan 输出路径
- 不影响 SELECT 查询结果的正确性（只影响 EXPLAIN 行数显示）