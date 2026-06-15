#!/usr/bin/env python3
"""题目四测试脚本：EXPLAIN ANALYZE + 选择下推 + 投影下推 (精确格式验证)"""
import socket, time, subprocess, os, sys, re

BUILD_DIR = '/home/zyc/db2026/build'
passed, failed = 0, 0

def do(s, sql, wait=0.3):
    s.sendall((sql + '\n').encode()); time.sleep(wait)
    s.settimeout(5.0)
    try:
        data = s.recv(65536)
        return data.decode('utf-8', errors='replace')
    except Exception:
        return '(error)'

def check(name, condition, detail=""):
    global passed, failed
    if condition:
        passed += 1; print(f'[PASS] {name}')
    else:
        failed += 1; print(f'[FAIL] {name}: {detail}')

# ============================================================
DB = '/tmp/rmdb_topic4'
subprocess.run(['rm', '-rf', DB])
p = subprocess.Popen([f'{BUILD_DIR}/bin/rmdb', DB],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2.0)
s = socket.socket(); s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
s.connect(('localhost', 8765)); time.sleep(0.2)

# ============================================================
# 测试点1: 单表查询 EXPLAIN ANALYZE (与赛题3.1一致)
# ============================================================
print('\n=== 测试点1: 单表查询 EXPLAIN ANALYZE ===')

do(s, 'CREATE TABLE t (a INT, b INT);')
do(s, 'INSERT INTO t VALUES (1, 5);')
do(s, 'INSERT INTO t VALUES (2, 8);')
do(s, 'INSERT INTO t VALUES (3, 12);')
do(s, 'INSERT INTO t VALUES (4, 6);')
do(s, 'INSERT INTO t VALUES (5, 20);')

# 普通SELECT验证
r = do(s, 'SELECT a, b FROM t WHERE a > 1 AND b < 10;')
check("1a. Simple SELECT", '2' in r and '8' in r and '4' in r and '6' in r,
      f"expected 2,8,4,6 got {repr(r[:100])}")

# EXPLAIN ANALYZE - 精确格式检查
r = do(s, 'EXPLAIN ANALYZE SELECT a, b FROM t WHERE a > 1 AND b < 10;')
lines = [l for l in r.split('\n') if l.strip()]
check("1b. has 3 plan lines", len(lines) >= 3,
      f"got {len(lines)} lines: {lines}")

# 检查每行精确格式
if len(lines) >= 3:
    l0 = lines[0].rstrip('\x00').strip()
    l1 = lines[1].rstrip('\x00') if len(lines) > 1 else ""
    l2 = lines[2].rstrip('\x00') if len(lines) > 2 else ""

    # Project行
    check("1c. Project rows=2", 'Project(columns=[' in l0 and 'rows=2)' in l0,
          f"got: {l0}")
    # Filter行 (缩进1层)
    check("1d. Filter indent", l1.startswith('\t') and not l1.startswith('\t\t'),
          f"got: {repr(l1)}")
    check("1e. Filter rows=2", 'Filter(condition=[' in l1 and 'rows=2)' in l1,
          f"got: {l1}")
    # Scan行 (缩进2层)
    check("1f. Scan indent", l2.startswith('\t\t') and not l2.startswith('\t\t\t'),
          f"got: {repr(l2)}")
    check("1g. Scan rows=5", 'Scan(table=t,' in l2 and 'rows=5)' in l2,
          f"got: {l2}")

# 验证output.txt内容
out_path = os.path.join(DB, 'output.txt')
if os.path.exists(out_path):
    with open(out_path) as f:
        out_content = f.read()
    check("1h. output.txt has EXPLAIN", 'Project(columns=[' in out_content,
          f"content: {repr(out_content[:100])}")
else:
    check("1h. output.txt exists", False, "file not found")

# ============================================================
# 测试点2: 选择运算下推 (使用JOIN ON语法 + 别名，与赛题3.2一致)
# ============================================================
print('\n=== 测试点2: 选择运算下推 ===')

do(s, 'CREATE TABLE orders (order_id INT, customer_id INT, order_date CHAR(40), total_amount FLOAT);')
do(s, 'CREATE TABLE customers (customer_id INT, name CHAR(50), email CHAR(100), address CHAR(200));')
do(s, "INSERT INTO customers VALUES (1, 'Alice', 'alice@example.com', 'A Street');")
do(s, "INSERT INTO customers VALUES (2, 'Bob', 'bob@example.com', 'B Street');")
do(s, "INSERT INTO customers VALUES (3, 'Carol', 'carol@example.com', 'C Street');")
do(s, "INSERT INTO orders VALUES (101, 1, '2025-01-01', 500.0);")
do(s, "INSERT INTO orders VALUES (102, 1, '2025-01-02', 1200.0);")
do(s, "INSERT INTO orders VALUES (103, 2, '2025-01-03', 900.0);")
do(s, "INSERT INTO orders VALUES (104, 2, '2025-01-04', 1500.0);")
do(s, "INSERT INTO orders VALUES (105, 3, '2025-01-05', 700.0);")

# SELECT验证 (用JOIN ON语法)
r = do(s, 'SELECT * FROM customers c JOIN orders o ON c.customer_id = o.customer_id WHERE o.total_amount > 1000;')
check("2a. JOIN ON SELECT", 'Alice' in r and 'Bob' in r and '1200.000000' in r and '1500.000000' in r,
      f"got: {repr(r[:200])}")

# EXPLAIN ANALYZE
r = do(s, 'EXPLAIN ANALYZE SELECT * FROM customers c JOIN orders o ON c.customer_id = o.customer_id WHERE o.total_amount > 1000;')
lines = [l.rstrip('\x00') for l in r.split('\n') if l.strip()]
print(f"  EXPLAIN lines: {len(lines)}")
for i, l in enumerate(lines):
    print(f"  [{i}]: {repr(l)}")

if len(lines) >= 6:
    check("2b. Project rows=2", 'rows=2)' in lines[0],
          f"got: {lines[0]}")
    # Join行
    check("2c. Join present", lines[1].startswith('\t') and 'Join(tables=[' in lines[1],
          f"got: {lines[1]}")
    check("2d. Join rows=2", 'rows=2)' in lines[1],
          f"got: {lines[1]}")
    # 左侧Scan (2层缩进)
    check("2e. Left Scan indent", lines[2].startswith('\t\t') and not lines[2].startswith('\t\t\t'),
          f"got: {repr(lines[2])}")
    check("2f. Left Scan customers", 'Scan(table=customers' in lines[2] and 'rows=3)' in lines[2],
          f"got: {lines[2]}")
    # Filter below Join (2层缩进)
    check("2g. Filter present", 'Filter(condition=[' in lines[3],
          f"got: {lines[3]}")
    check("2h. Filter rows=6", 'rows=6)' in lines[3],
          f"got: {lines[3]}")
    # 右侧Scan (3层缩进)
    check("2i. Right Scan orders", 'Scan(table=orders' in lines[4] and 'rows=15)' in lines[4],
          f"got: {lines[4]}")

# ============================================================
# 测试点3: 投影下推
# ============================================================
print('\n=== 测试点3: 投影下推 ===')

do(s, 'DROP TABLE orders;')
do(s, 'DROP TABLE customers;')
do(s, 'CREATE TABLE orders (order_id INT, customer_id INT, order_date CHAR(40), total_amount FLOAT);')
do(s, 'CREATE TABLE customers (customer_id INT, name CHAR(50), email CHAR(100), address CHAR(200));')
do(s, "INSERT INTO customers VALUES (1, 'Alice', 'alice@example.com', 'A Street');")
do(s, "INSERT INTO customers VALUES (2, 'Bob', 'bob@example.com', 'B Street');")
do(s, "INSERT INTO customers VALUES (3, 'Carol', 'carol@example.com', 'C Street');")
do(s, "INSERT INTO orders VALUES (101, 1, '2025-01-01', 500.0);")
do(s, "INSERT INTO orders VALUES (102, 1, '2025-01-02', 1200.0);")
do(s, "INSERT INTO orders VALUES (103, 2, '2025-01-03', 900.0);")
do(s, "INSERT INTO orders VALUES (104, 2, '2025-01-04', 1500.0);")
do(s, "INSERT INTO orders VALUES (105, 3, '2025-01-05', 700.0);")

# SELECT验证
r = do(s, 'SELECT c.name, o.order_id FROM customers c JOIN orders o ON c.customer_id = o.customer_id;')
check("3a. Projection SELECT", 'Alice' in r and 'Carol' in r and 'Total record(s): 5' in r,
      f"got: {repr(r[:200])}")

# EXPLAIN ANALYZE
r = do(s, 'EXPLAIN ANALYZE SELECT c.name, o.order_id FROM customers c JOIN orders o ON c.customer_id = o.customer_id;')
lines = [l.rstrip('\x00') for l in r.split('\n') if l.strip()]
print(f"  EXPLAIN lines: {len(lines)}")
for i, l in enumerate(lines):
    print(f"  [{i}]: {repr(l)}")

if len(lines) >= 7:
    check("3b. Project rows=5", 'rows=5)' in lines[0],
          f"got: {lines[0]}")
    check("3c. Join present", 'Join(tables=[' in lines[1],
          f"got: {lines[1]}")
    check("3d. Join rows=5", 'rows=5)' in lines[1],
          f"got: {lines[1]}")
    # 左侧投影下推
    check("3e. Left Project below Join", 'Project(columns=[' in lines[2],
          f"got: {lines[2]}")
    check("3f. Left Scan rows=3", 'Scan(table=customers' in lines[3] and 'rows=3)' in lines[3],
          f"got: {lines[3]}")
    # 右侧投影下推
    check("3g. Right Project below Join", 'Project(columns=[' in lines[4],
          f"got: {lines[4]}")
    check("3h. Right Scan rows=15", 'Scan(table=orders' in lines[5] and 'rows=15)' in lines[5],
          f"got: {lines[5]}")

# ============================================================
s.close()
p.terminate(); p.wait()

# Check output.txt
out_path = os.path.join(DB, 'output.txt')
print(f'\n=== output.txt: {"EXISTS" if os.path.exists(out_path) else "MISSING"} ===')
print(f'=== {passed} passed, {failed} failed ===')
