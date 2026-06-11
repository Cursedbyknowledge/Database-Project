#!/usr/bin/env python3
import socket, time, subprocess, os

DB = '/tmp/dbg_ci2'
subprocess.run(['rm', '-rf', DB])
p = subprocess.Popen(['build/bin/rmdb', DB], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)

s = socket.socket(); s.connect(('localhost', 8765)); time.sleep(0.2)

def run(sql, expect=False):
    s.sendall((sql + '\n').encode()); time.sleep(0.4)
    if expect:
        s.settimeout(0.5)
        try: return s.recv(8192).decode()
        except socket.timeout: return '(timeout)'
    return ''

run('CREATE TABLE t1 (id INT, val INT);')
run('INSERT INTO t1 VALUES (1, 100);')
run('INSERT INTO t1 VALUES (2, 200);')
run('INSERT INTO t1 VALUES (3, 300);')

print('=== Before index ===')
r = run('SELECT * FROM t1;', True)
print(r)

print('=== CREATE INDEX ===')
run('CREATE INDEX t1(val);')

print('=== After index (SELECT *) ===')
r = run('SELECT * FROM t1;', True)
print(r)

print('=== Index scan: val=200 ===')
r = run('SELECT * FROM t1 WHERE val = 200;', True)
print(r)

print('=== Seq scan: id=3 ===')
r = run('SELECT * FROM t1 WHERE id = 3;', True)
print(r)

print('=== output.txt ===')
op = os.path.join(DB, 'output.txt')
if os.path.exists(op):
    with open(op) as f:
        print(f.read())
s.close(); p.terminate(); p.wait()
