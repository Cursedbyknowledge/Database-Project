#!/bin/bash
# RMDB SQL 集成测试脚本 - 在 WSL 中运行
# 测试题目一~三的核心功能

set -e

PROJECT_DIR="/home/zyc/db2026"
BUILD_DIR="$PROJECT_DIR/build"
DB_NAME="test_db"

# 清理旧数据
rm -rf "$BUILD_DIR/$DB_NAME"
rm -f "$BUILD_DIR/$DB_NAME/output.txt"

echo "=== RMDB SQL 集成测试 ==="
echo ""

# 1. 启动服务端（后台运行）
echo "[1] 启动 RMDB 服务端 (数据库: $DB_NAME)..."
cd "$BUILD_DIR"
./bin/rmdb "$DB_NAME" &
SERVER_PID=$!
sleep 1
echo "    服务端 PID: $SERVER_PID"

# 辅助函数：发送 SQL 并等待结果
send_sql() {
    local sql="$1"
    local desc="$2"
    echo ""
    echo "--- $desc ---"
    echo "    SQL: $sql"
    echo "$sql" | timeout 3 nc localhost 8765 2>&1 || echo "    (timeout or no response)"
    sleep 0.2
}

# 2. 测试 DDL
send_sql "CREATE TABLE t1 (a INT, b INT, c CHAR(20));" "创建表 t1(a INT, b INT, c CHAR(20))"

send_sql "SHOW TABLES;" "显示所有表"

# 3. 测试 DML - INSERT
send_sql "INSERT INTO t1 VALUES (1, 10, 'hello');" "插入记录 (1, 10, 'hello')"
send_sql "INSERT INTO t1 VALUES (2, 20, 'world');" "插入记录 (2, 20, 'world')"
send_sql "INSERT INTO t1 VALUES (3, 30, 'rmdb');" "插入记录 (3, 30, 'rmdb')"

# 4. 测试 DQL - SELECT
send_sql "SELECT * FROM t1;" "SELECT * (全表扫描)"

send_sql "SELECT a, b FROM t1 WHERE a = 2;" "SELECT 条件过滤"

# 5. 测试 题目三 - CREATE INDEX + SELECT
send_sql "CREATE INDEX t1(a);" "创建索引 on t1(a)"

send_sql "SELECT * FROM t1 WHERE a = 1;" "索引扫描查询 a=1"

send_sql "SHOW INDEX FROM t1;" "显示索引信息"

# 6. 测试 DESC
send_sql "DESC t1;" "显示表结构"

# 7. 测试 DELETE
send_sql "DELETE FROM t1 WHERE a = 2;" "删除 a=2 的记录"

send_sql "SELECT * FROM t1;" "删除后查询(验证索引一致性)"

# 8. 测试 UPDATE
send_sql "UPDATE t1 SET b = 99 WHERE a = 1;" "更新记录"

send_sql "SELECT * FROM t1 WHERE a = 1;" "更新后查询"

# 9. 测试 DROP INDEX
send_sql "DROP INDEX t1(a);" "删除索引"

send_sql "SHOW INDEX FROM t1;" "删除索引后显示(应为空)"

# 10. 测试 DROP TABLE
send_sql "DROP TABLE t1;" "删除表"

send_sql "SHOW TABLES;" "删除表后显示(应为空)"

# 停止服务端
echo ""
echo "=== 测试完成，停止服务端 ==="
kill $SERVER_PID 2>/dev/null || true
sleep 0.5

# 检查 output.txt
echo ""
echo "=== output.txt 内容 ==="
cat "$BUILD_DIR/$DB_NAME/output.txt" 2>/dev/null || echo "output.txt 未生成"
