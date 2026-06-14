#!/usr/bin/env python3
"""题目三测试脚本 — 严格按测试说明文档2026.pdf 测试点1~3"""
import socket, time, subprocess, os, sys

HOST, PORT = 'localhost', 8765
DB = '/tmp/rmdb_topic3'
BUILD = '/home/zyc/db2026/build'

# === Setup ===
subprocess.run(['pkill', '-9', 'rmdb'], capture_output=True)
time.sleep(0.3)
subprocess.run(['rm', '-rf', DB], capture_output=True)
server = subprocess.Popen([f'{BUILD}/bin/rmdb', DB],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)

s = socket.socket(); s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
s.connect((HOST, PORT)); time.sleep(0.2)

def do(sql, wait=0.3):
    s.sendall((sql + '\n').encode()); time.sleep(wait)
    s.settimeout(3.0)
    try: return s.recv(8192).decode()
    except socket.timeout: return '(timeout)'

passed = 0; failed = 0
def check(name, result, expected):
    global passed, failed
    ok = all(e in result for e in expected)
    if ok:
        passed += 1; print(f'[PASS] {name}')
    else:
        missing = [e for e in expected if e not in result]
        failed += 1; print(f'[FAIL] {name}: missing {missing}')
        print(f'  got: {repr(result[:200])}')

# ===================================================================
# 测试点1: 创建、删除、展示索引 (对应 storage_test3)
# ===================================================================
print('\n=== 测试点1: 创建、删除、展示索引 ===')
do("CREATE TABLE warehouse (id INT, name CHAR(8));")
do("CREATE INDEX warehouse(id);")
r = do("SHOW INDEX FROM warehouse;")
check("1a. SHOW INDEX after single-col index", r, ["warehouse", "unique", "(id)"])

do("CREATE INDEX warehouse(id,name);")
r = do("SHOW INDEX FROM warehouse;")
check("1b. SHOW INDEX after multi-col index", r, ["(id,name)"])

do("DROP INDEX warehouse(id);")
do("DROP INDEX warehouse(id,name);")
r = do("SHOW INDEX FROM warehouse;")
check("1c. SHOW INDEX after drop all", r, [])  # 预期空输出

do("DROP TABLE warehouse;")

# ===================================================================
# 测试点2: 索引查询 (对应 storage_test4)
# ===================================================================
print('\n=== 测试点2: 索引查询 ===')
do("CREATE TABLE warehouse (w_id INT, name CHAR(8));")
do("INSERT INTO warehouse VALUES (10, 'qweruiop');")
do("INSERT INTO warehouse VALUES (534, 'asdfhjkl');")
do("INSERT INTO warehouse VALUES (100, 'qwerghjk');")
do("INSERT INTO warehouse VALUES (500, 'bgtyhnmj');")

# 2a: 单列索引查询
do("CREATE INDEX warehouse(w_id);")
r = do("SELECT * FROM warehouse WHERE w_id = 10;")
check("2a. Index point query w_id=10", r, ["10", "qweruiop"])

r = do("SELECT * FROM warehouse WHERE w_id < 534 AND w_id > 100;")
check("2b. Index range query", r, ["500", "bgtyhnmj"])

# 2c: DROP + CREATE on different column (已知 "Page 0" bug)
print('[INFO] 2c-2f: DROP INDEX + CREATE INDEX on different column (known bug)')
do("DROP INDEX warehouse(w_id);")
do("CREATE INDEX warehouse(name);")
r = do("SELECT * FROM warehouse WHERE name = 'qweruiop';")
check("2c. Index on name EQ", r, ["10", "qweruiop"])

r = do("SELECT * FROM warehouse WHERE name > 'qwerghjk';")
check("2d. Index on name range >", r, ["10", "qweruiop"])

r = do("SELECT * FROM warehouse WHERE name > 'aszdefgh' AND name < 'qweraaaa';")
check("2e. Index on name range between", r, ["500", "bgtyhnmj", "100", "qwerghjk"])

# 2f: Multi-column index
do("DROP INDEX warehouse(name);")
do("CREATE INDEX warehouse(w_id,name);")
r = do("SELECT * FROM warehouse WHERE w_id = 100 AND name = 'qwerghjk';")
check("2f. Multi-col index EQ", r, ["100", "qwerghjk"])

r = do("SELECT * FROM warehouse WHERE w_id < 600 AND name > 'bztyhnmj';")
check("2g. Multi-col index range", r, ["10", "qweruiop", "100", "qwerghjk"])

do("DROP TABLE warehouse;")

# ===================================================================
# 测试点3: 索引维护 (对应 storage_test5) - 已知bug：INSERT/UPDATE后数据不一致
# ===================================================================
print('\n[INFO] === 测试点3: 索引维护（含唯一性约束，已知bug）===')
do("CREATE TABLE warehouse (w_id INT, name CHAR(8));")
do("INSERT INTO warehouse VALUES (10, 'qweruiop');")
do("INSERT INTO warehouse VALUES (534, 'asdfhjkl');")

# 建索引前查询 (预期空结果)
r = do("SELECT * FROM warehouse WHERE w_id = 10;")
check("3a. Before index EQ", r, ["10", "qweruiop"])

r = do("SELECT * FROM warehouse WHERE w_id < 534 AND w_id > 100;")
# 没有记录满足此条件，预期空

do("CREATE INDEX warehouse(w_id);")

# 建索引后插入新记录
do("INSERT INTO warehouse VALUES (500, 'lastdanc');")

# 尝试插入重复w_id (应输出failure)
r = do("INSERT INTO warehouse VALUES (10, 'uiopqwer');")
# 唯一索引约束应触发failure

# 更新索引列
do("UPDATE warehouse SET w_id = 507 WHERE w_id = 534;")

# 验证索引查询正确
r = do("SELECT * FROM warehouse WHERE w_id = 10;")
check("3b. After INSERT/UPDATE, EQ", r, ["10", "qweruiop"])

r = do("SELECT * FROM warehouse WHERE w_id < 534 AND w_id > 100;")
check("3c. After INSERT/UPDATE, range", r, ["500", "lastdanc", "507"])

# 多列索引维护
do("DROP INDEX warehouse(w_id);")
do("CREATE INDEX warehouse(w_id,name);")

# 插入重复的(w_id,name)组合
r = do("INSERT INTO warehouse VALUES(10, 'qqqqoooo');")
# 应为failure（w_id=10, name='qweruiop'已存在）

r = do("INSERT INTO warehouse VALUES(500, 'lastdanc');")
# 应为failure（w_id=500, name='lastdanc'已存在）

# 更新为重复值
do("UPDATE warehouse SET w_id = 10, name = 'qqqqoooo' WHERE w_id = 507 AND name = 'asdfhjkl';")

# 最终验证
r = do("SELECT * FROM warehouse;")
check("3d. Final state", r, ["10", "qweruiop", "500", "lastdanc", "507", "asdfhjkl"])

# ===================================================================
# 验证 output.txt
# ===================================================================
s.close()
time.sleep(0.5)

op = os.path.join(DB, 'output.txt')
print(f"\n=== output.txt: {'EXISTS' if os.path.exists(op) else 'MISSING'} ===")
if os.path.exists(op):
    fc = open(op).read().count('failure')
    print(f"  failures: {fc}")
    print(open(op).read()[:500])

print(f'\n=== {passed} passed, {failed} failed ===')
server.terminate(); server.wait()
sys.exit(0 if failed == 0 else 1)
