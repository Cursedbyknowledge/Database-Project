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

## SQL 执行流水线

每个 SQL 语句经过以下阶段，形成从客户端到存储引擎的完整流水线：

```
客户端 (socket) → rmdb.cpp 监听端口 8765
  → Parser (flex lex.l + bison yacc.y): SQL 文本 → AST 节点 (ast.h)
  → Analyze (analyze/): AST → Query 结构（语义检查，类型校验）
  → Planner (optimizer/planner.cpp): Query → 逻辑 Plan (plan.h)
  → Optimizer (optimizer/): Plan 重写，选择 SeqScan vs IndexScan
  → Portal (portal.h): Plan → Executor 树 (AbstractExecutor*)
  → QlManager: 执行 executor 树，读写记录和索引
  → 结果序列化 → 客户端
```

DDL/DML/UTILITY 查询的流水线分叉：
- **DDL** (CREATE/DROP TABLE, CREATE/DROP INDEX): 由 `SmManager` 直接处理，操作 `fhs_`/`ihs_`/元数据，走 `PORTAL_MULTI_QUERY` 路径
- **DML SELECT**: Portal 将 `ProjectionPlan→ScanPlan` 转换为 `ProjectionExecutor + SeqScanExecutor/IndexScanExecutor`，走 `PORTAL_ONE_SELECT` 路径
- **DML INSERT/UPDATE/DELETE**: Portal 构建 InsertExecutor/UpdateExecutor/DeleteExecutor，走 `PORTAL_DML_WITHOUT_SELECT` 路径。UPDATE/DELETE 需遍历文件收集全部 Rid，然后逐个处理记录
- **UTILITY** (SHOW TABLES, DESC, SHOW INDEX): 走 `PORTAL_CMD_UTILITY` 路径

每条 SQL 都包裹在事务中（`make_txn()` + `commit_txn()` / `abort_txn()`），WAL 日志在写操作期间写入。

## 架构约定

### 全局管理器

`rmdb.cpp` 中使用 `std::unique_ptr` 持有全部全局管理器单例，按依赖顺序初始化：

1. `DiskManager` → 2. `BufferPoolManager` → 3. `RmManager`, `IxManager` → 4. `SmManager` → 5. `LockManager` → 6. `TransactionManager` → 7. `LogManager` → 8. `Planner` → 9. `Optimizer` → 10. `QlManager`

实际初始化代码结构（`src/rmdb.cpp`）：

```cpp
// 1. 文件系统层
auto disk_mgr = std::make_unique<DiskManager>(db_name);
auto bpm = std::make_unique<BufferPoolManager>(...);
// 2. 数据访问层（记录 + 索引）
auto rm_manager = std::make_unique<RmManager>(disk_mgr.get(), bpm.get());
auto ix_manager = std::make_unique<IxManager>(disk_mgr.get(), bpm.get());
// 3. 元数据管理
auto sm_manager = std::make_unique<SmManager>(disk_mgr.get(), bpm.get(),
                                              rm_manager.get(), ix_manager.get());
// 4. 并发控制
auto lock_mgr = std::make_unique<LockManager>();
auto txn_mgr = std::make_unique<TransactionManager>(lock_mgr.get());
// 5. 日志与恢复
auto log_mgr = std::make_unique<LogManager>(disk_mgr.get(), bpm.get());
auto recovery = std::make_unique<RecoveryManager>(..., log_mgr.get(),
                                                   sm_manager.get(), txn_mgr.get());
// 6. 查询层
auto planner = std::make_unique<Planner>(sm_manager.get());
auto optimizer = std::make_unique<Optimizer>(planner.get(), sm_manager.get());
auto ql = std::make_unique<QlManager>(sm_manager.get(), txn_mgr.get(),
                                       lock_mgr.get(), log_mgr.get());
// 7. SQL 入口
auto portal = std::make_unique<Portal>(sm_manager.get());
```

此顺序至关重要——较晚的组件依赖于较早的组件，不可更改。

### 关键数据结构关系

**记录存储（从外到内）**：
```
Database → SmManager::db_ (DbMeta)
  DbMeta → tabs_ (map<string, TabMeta>)
    TabMeta → cols_ (vector<ColMeta>), indexes_ (vector<IndexMeta>)
SmManager → fhs_ (map<string, RmFileHandle>), ihs_ (map<string, IxIndexHandle>)
```

**记录寻址**：
- `Rid` (`defs.h`): `{page_no, slot_no}` — 唯一标识堆文件中的一条记录
- `RmFileHandle`: 管理记录文件页面（每页含 bitmap 槽位分配 + 多条记录）
- `RmScan` (实现 `RecScan`): 全表扫描迭代器，按 `page_no`、按 `slot_no` 遍历

**索引映射**：
- `IxIndexHandle`: B+树，将键值映射到 Rid(s)。每个索引文件对应一棵独立的 B+树
- `CREATE INDEX col ON tab`: 在 `IxIndexHandle` 中为每条现有记录插入 `(col_value → rid)` 条目

**计划树 (`plan.h`)**：
Plan 为多态基类，具体类型如下：

| Plan 类型 | tag | 用途 |
|-----------|-----|------|
| `ProjectionPlan` | — | SELECT 投影列表 + 子计划 |
| `ScanPlan` | `T_SeqScan` 或 `T_IndexScan` | 表名 + 条件 + 索引列（可选） |
| `JoinPlan` | — | 左右子计划 + 连接条件 |
| `SortPlan` | — | 子计划 + 排序列 + is_desc |
| `DMLPlan` | `T_select/T_Update/T_Delete/T_Insert` | 表名 + 条件 + VALUES/SET 子句 |
| `DDLPlan` | — | DDL 类型 + 表名 + 列定义/索引列 |

**条件表达式**：
`Condition` (`common/common.h`): `{lhs_col, rhs_col, lhs_val, rhs_val, op}` — WHERE 子句中的单个条件。
操作符：`OP_EQ`, `OP_NE`, `OP_LT`, `OP_GT`, `OP_LE`, `OP_GE`。
多条件（AND 连接）以 `std::vector<Condition>` 形式传递。

### 上下文传递

`Context` 结构（`src/common/context.h`）承载请求级上下文：
- `txn_` — 当前事务指针
- `log_mgr_` — 日志管理器指针
- `lock_mgr_` — 锁管理器指针
- `data_send_` / `offset_` — 结果序列化缓冲区
- `ellipsis_` — 输出截断标记

### 框架不变项

以下文件的**公共接口不可修改**（内部实现可改）：
- `defs.h`, `errors.h` — 基础类型与异常
- `common/context.h` — Context 结构体成员
- `storage/page.h` — Page 布局
- `record/rm_defs.h`, `index/ix_defs.h` — 记录/索引常量
- `optimizer/plan.h` — Plan 节点（可新增类型）
- `execution/execution_defs.h` — 执行器定义

**可自由修改的**：任意函数体、类私有成员、.cpp 实现文件。`rmdb.cpp` 完全可定制。

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

```bash
# 运行全部单元测试
./build/bin/unit_test

# 按模块筛选运行
./build/bin/unit_test --gtest_filter="StorageTest.*"
./build/bin/unit_test --gtest_filter="LRUReplacerTest.*"
./build/bin/unit_test --gtest_filter="RecordManagerTest.*"
./build/bin/unit_test --gtest_filter="BufferPoolManagerTest.*"
```

### E2E 集成测试（Python，基于 Socket）

测试脚本通过 TCP 端口 8765 连接运行中的 RMDB 服务器，发送 SQL 语句并验证返回结果。
每个测试独立启动 RMDB 进程并创建独立数据库目录（`/tmp/rmdb_*`），互不冲突。

| 脚本 | 用途 | 连接方式 |
|------|------|----------|
| `test_oneconn.py` | 单持久连接，测试全部基本 SQL | 一个 socket，顺序发送 |
| `test_rmdb.py` | 每条 SQL 独立连接，含严格断言 | 每次 `send_sql()` 创建新 socket |
| `test_final.py` | 类竞赛 E2E：DDL、CRUD、UPDATE、DELETE、索引创建与范围查询 | 单连接 |
| `test_idx_e2e.py` | 索引专项：创建、展示、等值/范围扫描、删除重建 | 单连接 |
| `test_char_update.py` | CHAR 字段上的 UPDATE 端到端测试 | 单连接 |
| `test_index_scan.py` | 索引扫描快速冒烟测试 | 单连接 |

```bash
# 运行任意 E2E 测试（需先编译完成）
cd /home/zyc/db2026
python3 test_final.py         # 类竞赛全流程
python3 test_idx_e2e.py      # 索引专项
python3 test_oneconn.py      # 单连接全量测试
```

### Shell 辅助脚本

- `build_and_test.sh`: 完整流水线——环境检查 → 重新生成 parser → cmake → make → 运行选定 gtest 套件
- `test_sql.sh`: 启动服务器，通过 nc 发送 SQL，打印 `output.txt`，用于手动验证

### 集成测试（正式赛题）

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

1.  **赛题全文**（`数据库管理系统设计大赛赛题全文.txt`）
2.  **代码注释**（最直接、最权威的接口说明）
3.  **项目结构文档**（`RMDB项目结构.pdf`）
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

## 已知 TODO 区域与待实现功能

以下是框架中标记为 `// Todo:` 的关键位置，属于赛题核心得分点：

| 模块 | 文件 / 位置 | 需要实现的内容 |
|------|------------|---------------|
| **索引层** | `index/ix_index_handle.cpp` | `insert_entry()`, `delete_entry()`, B+树 split/merge、`search()` |
| **系统管理** | `system/sm_manager.cpp` | `create_table()`, `drop_table()`, `create_index()`, `drop_index()` |
| **记录管理** | `record/rm_file_handle.cpp` | `insert_record()`, `delete_record()`, `update_record()`, bitmap 槽位管理 |
| **事务** | `transaction/` | `TransactionManager`：事务生命周期、并发控制 |
| **日志恢复** | `recovery/` | `LogManager`：WAL 日志写入；`RecoveryManager`：崩溃恢复 |
| **执行器** | `execution/executor_update.h` | UPDATE 后维护所有受影响索引（删旧键+插新键） |
| **执行器** | `execution/executor_delete.h` | DELETE 后从所有受影索引中移除条目 |
| **执行器** | `execution/executor_index_scan.h` | 利用 B+树范围扫描实现索引查找 |
| **执行器** | `execution/executor_nestedloop_join.h` | 嵌套循环连接算法 |
| **优化器** | `optimizer/planner.cpp` | 选择 IndexScan vs SeqScan 的优化逻辑 |

在修改这些文件前，务必先阅读头文件中的接口注释和返回值约定。

## 构建系统细微之处

### 重新生成 SQL 解析器

SQL 解析器文件（`src/parser/lex.yy.cpp`, `yacc.tab.cpp`, `yacc.tab.h`, `yacc.tab.hpp`）由 flex/bison 生成：

- `src/parser/lex.l` — flex 词法规则（关键词、标识符、数字、字符串等）
- `src/parser/yacc.y` — bison 语法规则（SQL 语句文法）

修改 SQL 语法后手动重新生成：
```bash
cd src/parser
bison -d -o yacc.tab.cpp yacc.y
flex -o lex.yy.cpp lex.l
```

生成的 `.cpp` 文件已提交到仓库——即使未安装 flex/bison 也可直接编译。但如果修改了 `.l`/`.y` 文件而未重新生成，会导致语法解析错误。

### 编译选项说明

根 `CMakeLists.txt` 中默认 Debug 模式：`-Wall -O0 -g -ggdb3`。
性能测试时切换 Release：取消注释 `# set(CMAKE_CXX_FLAGS "-Wall -O3")`。

### 依赖库

`deps/` 目录包含 Google Test 等第三方依赖，通过 `add_subdirectory(deps)` 递归编译。主目标 `rmdb` 链接 `pthread` 和 `readline`。

## 常见问题排查

### 服务器启动失败或端口冲突
```bash
pkill -9 rmdb          # 杀掉所有旧 RMDB 进程
lsof -i :8765          # 检查端口是否被占用
```

### 段错误 (SIGSEGV) 调试
```bash
# Debug 构建 + AddressSanitizer（修改 CMakeLists.txt 添加 -fsanitize=address）
cmake .. -DCMAKE_CXX_FLAGS="-Wall -O0 -g -fsanitize=address"
make -j$(nproc)
# 启用 core dump 后用 gdb 回溯
ulimit -c unlimited
gdb ./bin/rmdb core
```

### 输出数据中缺失或不正确的记录
1. 检查 `build/<db_name>/output.txt` 中是否包含 `failure` 关键词
2. 验证索引 B+树在 INSERT/UPDATE/DELETE 操作后是否与记录文件保持一致——这是最常见的 bug 来源
3. 确认 `SmManager::fhs_`（记录文件句柄）和 `SmManager::ihs_`（索引文件句柄）均已正确初始化

### UPDATE/DELETE 后索引不一致
这是竞赛中最容易失分的区域。UpdateExecutor 和 DeleteExecutor 必须在修改/删除记录后更新所有受影响的索引：
- **UPDATE**：从索引中删除旧键值对 `(old_value → rid)`，插入新键值对 `(new_value → rid)`
- **DELETE**：从索引中删除该记录对应的所有键值对
- 必须遍历 `TabMeta::indexes_` 获取所有受影响的索引列表

### 事务提交失败或死锁
- 检查 LockManager 中行级锁的加锁/解锁顺序
- 确保 `make_txn()` 与 `commit_txn()`/`abort_txn()` 成对调用
- 确认 WAL 日志在所有写操作之前/之后正确写入
