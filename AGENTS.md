# AGENTS.md — RMDB 数据库竞赛开发规范

## 项目概述

RMDB（Renmin Mini Database）是全国大学生计算机系统能力大赛·数据库管理系统设计赛的代码框架。基于 C++17、CMake 构建，部署于 Ubuntu 18.04+ (64位)。

- **上游框架**：https://gitlab.eduxiji.net/csc1/csc-db/db2026/-/tree/main/rmdb
- **推送仓库**：https://gitlab.eduxiji.net/T2026110629910427/rmdb-lmsycbk-project.git
- **工作分支**：`ZYC`
- **许可证**：木兰宽松许可证 v2

## 环境要求

- GCC ≥ 7.1（完全支持 C++17）
- CMake ≥ 3.16
- flex + bison（SQL 解析器生成）
- readline 库（命令行交互）
- pthread（并发控制）

Ubuntu 环境安装：

```bash
sudo apt update
sudo apt install -y build-essential cmake flex bison libreadline-dev
```

## 构建与运行

### 首次构建

```bash
cd rmdb
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### 后续增量编译

```bash
cd rmdb/build
make -j$(nproc)
```

### 运行服务端

```bash
# 服务端监听 8765 端口，客户端通过 socket 连接
./build/bin/rmdb <db_name>

# 示例：以 execution_test_db 数据库启动
./build/bin/rmdb execution_test_db
```

### 运行单元测试

```bash
./build/bin/unit_test

# 运行特定测试
./build/bin/unit_test --gtest_filter="StorageTest.*"
```

## 代码规范

### 命名约定

| 元素 | 风格 | 示例 |
|------|------|------|
| 文件名 | snake_case | `disk_manager.h`, `buffer_pool_manager.cpp` |
| 类名 | PascalCase | `DiskManager`, `BufferPoolManager` |
| 函数/方法 | snake_case | `write_page()`, `fetch_page()` |
| 成员变量 | snake_case + 尾缀 `_` | `page_no`, `fd2pageno_`, `latch_` |
| 常量/宏 | UPPER_SNAKE_CASE | `PAGE_SIZE`, `BUFFER_POOL_SIZE` |
| 命名空间 | snake_case | `ast` |

### 头文件

- 统一使用 `#pragma once` 作为头文件保护
- 头文件包含顺序：自身头文件 → 标准库 → 第三方库 → 项目内头文件
- 不在头文件中使用 `using namespace`

### 框架约束

- **严禁修改已有接口签名**（函数名、参数列表、返回类型）
- **严禁删除已有数据结构中的数据成员**
- **允许新增**接口、数据结构、变量
- 框架中 `// Todo:` 注释标记的位置是待实现代码的插入点
- 如需大幅度重构，优先考虑不影响外部接口的内部实现变更

### 格式化

- 缩进使用 4 个空格（非 Tab）
- 左大括号不换行
- 行宽建议 ≤ 120 字符
- 指针/引用符号靠近类型：`int* ptr`, `const std::string& name`

### 注释

- 公共接口使用 Doxygen 风格注释
- 实现中的关键逻辑加行内注释说明意图
- 不注释显而易见的代码

## 架构约定

### 全局管理器

`rmdb.cpp` 中使用 `std::unique_ptr` 持有全部全局管理器单例，按依赖顺序初始化：

1. `DiskManager` → 2. `BufferPoolManager` → 3. `RmManager`, `IxManager` → 4. `SmManager` → 5. `LockManager` → 6. `TransactionManager` → 7. `LogManager` → 8. `Planner` → 9. `Optimizer` → 10. `QlManager`

### 上下文传递

`Context` 结构（`src/common/context.h`）承载请求级上下文：
- `txn_` — 当前事务指针
- `log_mgr_` — 日志管理器指针
- `lock_mgr_` — 锁管理器指针

### 异常处理

全部异常继承自 `RMDBError`（`src/errors.h`）。遇到错误直接 `throw`，由上层 `Portal` 捕获并返回错误信息。

### 线程安全

- `BufferPoolManager` 的页表操作使用 `pthread_mutex_t` 保护
- `LRUReplacer` 使用 `std::scoped_lock` + `std::mutex latch_`
- `LockManager` 提供行级锁，支持共享锁/排他锁

## 测试规范

### 单元测试（GTest）

测试文件：`src/unit_test.cpp`。添加新测试时：

```cpp
TEST(TestSuiteName, TestName) {
    // 测试逻辑
    EXPECT_EQ(result, expected);
}
```

### 集成测试

- 测试用例见 `数据库管理系统设计大赛赛题全文.txt`
- 测试采用 SQL 语句通过客户端提交
- 输出写入 `build/<db_name>/output.txt`

### 编译 CMake 配置

`src/test/CMakeLists.txt` 控制测试编译。添加新测试源文件时需更新此文件。

## Git 工作流

### 分支策略

```bash
# 仅在 ZYC 分支上开发
git checkout ZYC

# 定期同步上游框架（按需）
git remote add upstream https://gitlab.eduxiji.net/csc1/csc-db/db2026.git
git fetch upstream main
git merge upstream/main
```

### 提交规范

- Commit message 格式：`[题目N] 简短描述`
- 示例：`[题目一] 实现DiskManager文件读写和页面管理`
- 每道题通过测试后提交一次，粒度适中

### 推送

```bash
git remote add eduxiji https://gitlab.eduxiji.net/T2026110629910427/rmdb-lmsycbk-project.git
git push eduxiji ZYC
```

## 参考资料优先级

1. **代码注释**（最直接、最权威的接口说明）
2. **赛题全文**（`数据库管理系统设计大赛赛题全文.txt`）
3. **项目结构文档**（`RMDB项目结构.pdf`）
4. **使用文档**（`RMDB使用文档.pdf`）
5. **环境配置文档**（`RMDB环境配置文档.pdf`）
6. **测试说明文档**（`测试说明文档2026.pdf`）
7. **数据一致性检验规则**（`数据一致性检验规则.pdf`）

## 关键文件索引

| 文件 | 作用 |
|------|------|
| `rmdb/src/rmdb.cpp` | 主入口，全局初始化，客户端处理循环 |
| `rmdb/src/defs.h` | 基础类型：Rid, ColType, RecScan |
| `rmdb/src/errors.h` | 全部异常类定义 |
| `rmdb/src/common/config.h` | 常量配置：PAGE_SIZE, BUFFER_POOL_SIZE 等 |
| `rmdb/src/system/sm_meta.h` | 元数据结构：DbMeta, TabMeta, ColMeta, IndexMeta |
| `rmdb/src/storage/page.h` | Page 结构定义 |
| `rmdb/src/record/rm_defs.h` | 记录管理常量与结构定义 |
| `rmdb/src/index/ix_defs.h` | 索引相关常量与结构定义 |
| `rmdb/src/execution/execution_defs.h` | 执行器通用定义 |
| `rmdb/src/optimizer/plan.h` | 计划节点定义 |
| `rmdb/src/parser/ast.h` | SQL AST 节点定义 |
| `rmdb/src/transaction/txn_defs.h` | 事务相关定义 |
| `rmdb/src/recovery/log_defs.h` | 日志结构定义 |
