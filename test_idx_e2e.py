#!/usr/bin/env python3
"""E2E test focused on index operations per competition spec"""
import socket, time, subprocess, os

DB = '/tmp/rmdb_idx_test'
subprocess.run(['rm', '-rf', DB])
p = subprocess.Popen(['build/bin/rmdb', DB], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)
s = socket.socket(); s.connect(('localhost', 8765)); time.sleep(0.2)

def do(sql, wait=0.3):
    s.sendall((sql + '\n').encode()); time.sleep(wait)
    s.settimeout(2.0)
    try: d = s.recv(8192); return d.decode()
    except socket.timeout: return '(timeout)'

# Test index creation and show
do("CREATE TABLE warehouse (w_id INT, name CHAR(8));")
do("CREATE INDEX warehouse(w_id);")
r = do("SHOW INDEX FROM warehouse;")
print("SHOW INDEX:", repr(r[:200]))

# Test index scan
do("INSERT INTO warehouse VALUES (10, 'qweruiop');")
do("INSERT INTO warehouse VALUES (534, 'asdfhjkl');")
do("INSERT INTO warehouse VALUES (100, 'qwerghjk');")
do("INSERT INTO warehouse VALUES (500, 'bgtyhnmj');")

r = do("SELECT * FROM warehouse WHERE w_id = 10;")
print("IDX w_id=10:", repr(r[:200]))

r = do("SELECT * FROM warehouse WHERE w_id < 534 AND w_id > 100;")
print("IDX range:", repr(r[:200]))

# Test drop index and re-create
do("DROP INDEX warehouse(w_id);")
do("CREATE INDEX warehouse(name);")
r = do("SELECT * FROM warehouse WHERE name = 'qweruiop';")
print("IDX name:", repr(r[:200]))

r = do("SHOW INDEX FROM warehouse;")
print("SHOW IDX:", repr(r[:200]))

# Check output.txt
op = os.path.join(DB, 'output.txt')
print(f"\noutput.txt exists: {os.path.exists(op)}")
if os.path.exists(op):
    c = open(op).read()
    print(f"failures: {c.count('failure')}")
    print(c[:500])

s.close(); p.terminate(); p.wait()
print("DONE")
