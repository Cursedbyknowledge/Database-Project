#!/usr/bin/env python3
import socket, time, subprocess, os

HOST, PORT = 'localhost', 8765

subprocess.run(['pkill', '-9', 'rmdb'], capture_output=True)
time.sleep(0.3)
subprocess.run(['rm', '-rf', '/tmp/rmdb_e2e_test'], capture_output=True)
p = subprocess.Popen(['build/bin/rmdb', '/tmp/rmdb_e2e_test'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)

s = socket.socket(); s.connect((HOST, PORT)); time.sleep(0.2)

def ok(sql):
    s.sendall((sql + '\n').encode()); time.sleep(0.3)
def sel(sql):
    s.sendall((sql + '\n').encode()); time.sleep(0.3)
    s.settimeout(0.5)
    try: return s.recv(8192).decode()
    except: return '(timeout)'

# Test 1: Basic CRUD
ok("CREATE TABLE grade (name CHAR(20), id INT, score FLOAT);")
ok("INSERT INTO grade VALUES ('Data Structure', 1, 90.5);")
ok("INSERT INTO grade VALUES ('Data Structure', 2, 95.0);")
ok("INSERT INTO grade VALUES ('Calculus', 2, 92.0);")
ok("INSERT INTO grade VALUES ('Calculus', 1, 88.5);")

print("=== TEST 1: SELECT * ===")
r = sel("SELECT * FROM grade;")
print(r[:400] if r else "EMPTY")

print("=== TEST 2: UPDATE ===")
ok("UPDATE grade SET score = 90 WHERE name = 'Calculus';")
r = sel("SELECT * FROM grade;")
print(r[:400] if r else "EMPTY")

print("=== TEST 3: DELETE ===")
ok("DELETE FROM grade WHERE score > 90;")
r = sel("SELECT * FROM grade;")
print(r[:400] if r else "EMPTY")

# Test 2: Index
ok("CREATE TABLE warehouse (w_id INT, name CHAR(8));")
ok("INSERT INTO warehouse VALUES (10, 'qweruiop');")
ok("INSERT INTO warehouse VALUES (534, 'asdfhjkl');")
ok("INSERT INTO warehouse VALUES (100, 'qwerghjk');")
ok("INSERT INTO warehouse VALUES (500, 'bgtyhnmj');")
ok("CREATE INDEX warehouse(w_id);")

print("=== TEST 4: Index SELECT w_id=10 ===")
r = sel("SELECT * FROM warehouse WHERE w_id = 10;")
print(r[:300] if r else "EMPTY")

print("=== TEST 5: Index range w_id < 534 AND w_id > 100 ===")
r = sel("SELECT * FROM warehouse WHERE w_id < 534 AND w_id > 100;")
print(r[:300] if r else "EMPTY")

print("=== TEST 6: SHOW INDEX ===")
r = sel("SHOW INDEX FROM warehouse;")
print(r[:300] if r else "EMPTY")

print("=== TEST 7: DROP INDEX ===")
ok("DROP INDEX warehouse(w_id);")
r = sel("SHOW INDEX FROM warehouse;")
print(r[:300] if r else "EMPTY")

s.close()

# Check output
db_dir = '/tmp/rmdb_e2e_test'
op = os.path.join(db_dir, 'output.txt')
print(f"\n=== output.txt ({'EXISTS' if os.path.exists(op) else 'MISSING'}) ===")
if os.path.exists(op):
    fc = open(op).read().count('failure')
    print(f"failures in output.txt: {fc}")
    print(open(op).read()[:500])

print(f"\nServer alive: {p.poll() is None}")
p.terminate(); p.wait()
print("DONE")
