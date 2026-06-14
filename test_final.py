#!/usr/bin/env python3
"""Full competition-style E2E test"""
import socket, time, subprocess, os

DB = '/tmp/rmdb_ci_test'
subprocess.run(['rm', '-rf', DB])
p = subprocess.Popen(['build/bin/rmdb', DB], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)

s = socket.socket(); s.connect(('localhost', 8765)); time.sleep(0.2)
def do(sql, wait=0.3):
    s.sendall((sql + '\n').encode()); time.sleep(wait)
    s.settimeout(2.0)
    try: d = s.recv(8192); return d.decode()
    except socket.timeout: return '(timeout)'

passed = 0; failed = 0
def check(name, result, expected_substrings):
    global passed, failed
    all_found = all(e in result for e in expected_substrings)
    if all_found:
        passed += 1; print(f'[PASS] {name}')
    else:
        failed += 1; print(f'[FAIL] {name}: missing {[e for e in expected_substrings if e not in result]}')

# === TEST 1: DDL ===
do("CREATE TABLE t1 (id INT, name CHAR(8));")
do("CREATE TABLE t2 (id INT);")
r = do("SHOW TABLES;")
check("SHOW TABLES", r, ["t1", "t2"])

do("DROP TABLE t1;")
r = do("SHOW TABLES;")
check("SHOW TABLES after DROP", r, ["t2"])
assert 't1' not in r, "t1 should be dropped"

do("DROP TABLE t2;")

# === TEST 2: DML CRUD ===
do("CREATE TABLE grade (name CHAR(20), id INT, score FLOAT);")
do("INSERT INTO grade VALUES ('Data Structure', 1, 90.5);")
do("INSERT INTO grade VALUES ('Data Structure', 2, 95.0);")
do("INSERT INTO grade VALUES ('Calculus', 2, 92.0);")
do("INSERT INTO grade VALUES ('Calculus', 1, 88.5);")

r = do("SELECT * FROM grade;")
check("SELECT *", r, ["Data Structure", "Calculus", "90.500000", "95.000000"])

r = do("SELECT score, name, id FROM grade WHERE score > 90;")
check("SELECT WHERE score>90", r, ["Data Structure", "Calculus", "90.500000", "95.000000", "92.000000"])

# === TEST 3: UPDATE (INT only, no CHAR) ===
do("CREATE TABLE t3 (a INT, b INT);")
do("INSERT INTO t3 VALUES (1, 10);")
do("INSERT INTO t3 VALUES (2, 20);")
do("UPDATE t3 SET b = 99 WHERE a = 1;")
r = do("SELECT * FROM t3 WHERE a = 1;")
check("UPDATE int", r, ["1", "99"])

# === TEST 4: DELETE (INT only) ===
do("DELETE FROM t3 WHERE a = 1;")
r = do("SELECT * FROM t3;")
check("DELETE", r, ["2", "20"])
assert ' 1 ' not in r and ' 10' not in r, "deleted record should not appear"

# === TEST 5: INDEX (创建、查询) ===
do("CREATE TABLE warehouse (w_id INT, name CHAR(8));")
do("INSERT INTO warehouse VALUES (10, 'qweruiop');")
do("INSERT INTO warehouse VALUES (534, 'asdfhjkl');")
do("INSERT INTO warehouse VALUES (100, 'qwerghjk');")
do("INSERT INTO warehouse VALUES (500, 'bgtyhnmj');")
do("CREATE INDEX warehouse(w_id);")

r = do("SHOW INDEX FROM warehouse;")
check("SHOW INDEX", r, ["warehouse", "unique", "w_id"])

r = do("SELECT * FROM warehouse WHERE w_id = 10;")
check("Index EQ", r, ["10", "qweruiop"])

r = do("SELECT * FROM warehouse WHERE w_id < 534 AND w_id > 100;")
check("Index range", r, ["500", "bgtyhnmj"])

# === TEST 6: DROP INDEX + CREATE INDEX on different column ===
# Note: 已知 "Page 0" bug, 以下测试预期失败
print("[INFO] TEST 6: DROP INDEX + CREATE on different column (known Page 0 bug)")
do("DROP INDEX warehouse(w_id);")
do("CREATE INDEX warehouse(name);")
r = do("SELECT * FROM warehouse WHERE name = 'qweruiop';")
check("Index on name column", r, ["10", "qweruiop"])

# === TEST 7: 索引维护 (INSERT/UPDATE after CREATE INDEX) ===
print("[INFO] TEST 7: Index maintenance (INSERT/UPDATE after CREATE INDEX)")
do("DROP TABLE warehouse;")
do("CREATE TABLE warehouse (w_id INT, name CHAR(8));")
do("INSERT INTO warehouse VALUES (10, 'qweruiop');")
do("INSERT INTO warehouse VALUES (534, 'asdfhjkl');")
do("CREATE INDEX warehouse(w_id);")
do("INSERT INTO warehouse VALUES (500, 'lastdanc');")
do("UPDATE warehouse SET w_id = 507 WHERE w_id = 534;")
r = do("SELECT * FROM warehouse WHERE w_id = 10;")
check("Index maintain EQ", r, ["10", "qweruiop"])
r = do("SELECT * FROM warehouse WHERE w_id < 534 AND w_id > 100;")
check("Index maintain range", r, ["500", "lastdanc", "507", "asdfhjkl"])

# === VERIFY output.txt ===
s.close()
time.sleep(0.5)

# output.txt 在数据库目录下（open_db执行chdir）
op = os.path.join(DB, 'output.txt')
print(f"\noutput.txt: {'EXISTS' if os.path.exists(op) else 'MISSING'}")
if os.path.exists(op):
    fc = open(op).read().count('failure')
    checks = [
        ("Contains 't1'", 't1' in open(op).read()),
        ("Contains 't2'", 't2' in open(op).read()),
        ("Contains grade data", 'Data Structure' in open(op).read()),
        ("Contains warehouse data", 'warehouse' in open(op).read()),
    ]
    for desc, ok in checks:
        print(f"  [{('PASS' if ok else 'FAIL')}] {desc}")
    print(f"  failures={fc}")

print(f"\n=== {passed} passed, {failed} failed ===")
p.terminate(); p.wait()
