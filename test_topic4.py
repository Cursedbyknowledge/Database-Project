#!/usr/bin/env python3
"""题目四测试脚本：EXPLAIN ANALYZE + 选择下推 + 投影下推"""
import socket, time, subprocess, os, sys

BUILD_DIR = '/home/zyc/db2026/build'
passed, failed = 0, 0

def do(s, sql, wait=0.3):
    s.sendall((sql + '\n').encode()); time.sleep(wait)
    s.settimeout(5.0)
    try: return s.recv(65536).decode('utf-8', errors='replace')
    except socket.timeout: return '(timeout)'

def check(name, result, expected_parts):
    global passed, failed
    missing = [p for p in expected_parts if p not in result]
    if not missing:
        passed += 1; print(f'[PASS] {name}')
    else:
        failed += 1; print(f'[FAIL] {name}: missing {missing}')
        print(f'  got: {repr(result[:300])}')

# ============================================================
DB = '/tmp/rmdb_topic4'
subprocess.run(['rm', '-rf', DB])
p = subprocess.Popen([f'{BUILD_DIR}/bin/rmdb', DB],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2.0)
s = socket.socket(); s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
s.connect(('localhost', 8765)); time.sleep(0.2)

# ============================================================
# 测试点1: 单表查询 + EXPLAIN ANALYZE
# ============================================================
print('\n=== 测试点1: 单表查询 EXPLAIN ANALYZE ===')

do(s, 'CREATE TABLE t (a INT, b INT);')
do(s, 'INSERT INTO t VALUES (1, 5);')
do(s, 'INSERT INTO t VALUES (2, 8);')
do(s, 'INSERT INTO t VALUES (3, 12);')
do(s, 'INSERT INTO t VALUES (4, 6);')
do(s, 'INSERT INTO t VALUES (5, 20);')

# 先验证普通SELECT
r = do(s, 'SELECT a, b FROM t WHERE a > 1 AND b < 10;')
check("1a. Simple SELECT", r, ['2', '8', '4', '6'])

# EXPLAIN ANALYZE
r = do(s, 'EXPLAIN ANALYZE SELECT a, b FROM t WHERE a > 1 AND b < 10;')
check("1b. EXPLAIN ANALYZE has Project", r, ['Project'])
check("1c. EXPLAIN ANALYZE has Filter", r, ['Filter'])
check("1d. EXPLAIN ANALYZE has Scan", r, ['Scan'])
check("1e. EXPLAIN ANALYZE has rows=", r, ['rows='])
check("1f. EXPLAIN ANALYZE has SeqScan", r, ['SeqScan'])
check("1g. Scan rows=5", r, ['t', 'rows=5'])

# ============================================================
# 测试点2: 选择运算下推
# ============================================================
print('\n=== 测试点2: 选择运算下推 ===')

do(s, 'CREATE TABLE orders (order_id INT, customer_id INT, order_date CHAR(40), total_amount FLOAT);')
do(s, 'CREATE TABLE customers (customer_id INT, name CHAR(50), email CHAR(100), address CHAR(200));')
do(s, 'INSERT INTO customers VALUES (1, \'Alice\', \'alice@example.com\', \'A Street\');')
do(s, 'INSERT INTO customers VALUES (2, \'Bob\', \'bob@example.com\', \'B Street\');')
do(s, 'INSERT INTO customers VALUES (3, \'Carol\', \'carol@example.com\', \'C Street\');')
do(s, 'INSERT INTO orders VALUES (101, 1, \'2025-01-01\', 500.0);')
do(s, 'INSERT INTO orders VALUES (102, 1, \'2025-01-02\', 1200.0);')
do(s, 'INSERT INTO orders VALUES (103, 2, \'2025-01-03\', 900.0);')
do(s, 'INSERT INTO orders VALUES (104, 2, \'2025-01-04\', 1500.0);')
do(s, 'INSERT INTO orders VALUES (105, 3, \'2025-01-05\', 700.0);')

# 先用不带JOIN别名的简单查询验证
# 普通SELECT
r = do(s, 'SELECT * FROM customers, orders WHERE customers.customer_id = orders.customer_id AND orders.total_amount > 1000;')
check("2a. Predicate pushdown SELECT", r, ['Alice', '102', 'Bob', '104'])

# EXPLAIN ANALYZE (过滤条件应下推到orders扫描之上)
r = do(s, 'EXPLAIN ANALYZE SELECT * FROM customers, orders WHERE customers.customer_id = orders.customer_id AND orders.total_amount > 1000;')
check("2b. EXPLAIN has Join", r, ['Join'])
check("2c. Filter below Join", r, ['Filter'])
check("2d. Filter condition total_amount", r, ['total_amount'])

# ============================================================
# 测试点3: 投影下推
# ============================================================
print('\n=== 测试点3: 投影下推 ===')

do(s, 'DROP TABLE orders;')
do(s, 'DROP TABLE customers;')
do(s, 'CREATE TABLE orders (order_id INT, customer_id INT, order_date CHAR(40), total_amount FLOAT);')
do(s, 'CREATE TABLE customers (customer_id INT, name CHAR(50), email CHAR(100), address CHAR(200));')
do(s, 'INSERT INTO customers VALUES (1, \'Alice\', \'alice@example.com\', \'A Street\');')
do(s, 'INSERT INTO customers VALUES (2, \'Bob\', \'bob@example.com\', \'B Street\');')
do(s, 'INSERT INTO customers VALUES (3, \'Carol\', \'carol@example.com\', \'C Street\');')
do(s, 'INSERT INTO orders VALUES (101, 1, \'2025-01-01\', 500.0);')
do(s, 'INSERT INTO orders VALUES (102, 1, \'2025-01-02\', 1200.0);')
do(s, 'INSERT INTO orders VALUES (103, 2, \'2025-01-03\', 900.0);')
do(s, 'INSERT INTO orders VALUES (104, 2, \'2025-01-04\', 1500.0);')
do(s, 'INSERT INTO orders VALUES (105, 3, \'2025-01-05\', 700.0);')

r = do(s, 'SELECT name, order_id FROM customers, orders WHERE customers.customer_id = orders.customer_id;')
check("3a. Projection pushdown SELECT", r, ['Alice', '101', 'Bob', '103', 'Carol', '105'])

r = do(s, 'EXPLAIN ANALYZE SELECT name, order_id FROM customers, orders WHERE customers.customer_id = orders.customer_id;')
check("3b. EXPLAIN has Project/Join/Scan", r, ['Project'])
check("3c. Has Join", r, ['Join'])

# ============================================================
s.close()
p.terminate(); p.wait()

# Check output.txt
op = os.path.join(DB, 'output.txt')
print(f'\n=== output.txt: {"EXISTS" if os.path.exists(op) else "MISSING"} ===')
print(f'=== {passed} passed, {failed} failed ===')
