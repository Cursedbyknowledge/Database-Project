#!/usr/bin/env python3
"""RMDB full integration test - single persistent connection"""
import socket, time, subprocess, os

HOST, PORT = 'localhost', 8765
DB = '/tmp/rmdb_oneconn'

subprocess.run(['pkill', '-9', 'rmdb'], capture_output=True)
time.sleep(0.3)
subprocess.run(['rm', '-rf', DB], capture_output=True)
p = subprocess.Popen(['build/bin/rmdb', DB], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)

s = socket.socket(); s.connect((HOST, PORT)); time.sleep(0.2)
total_p = 0; total_f = 0

def test(name, sql, expect=False, check=None):
    global total_p, total_f
    s.sendall((sql + '\n').encode())
    time.sleep(0.5)
    if expect:
        s.settimeout(1.5)
        try:
            data = s.recv(8192)
            result = data.decode('utf-8', errors='replace')
        except socket.timeout:
            result = ''
    else:
        result = ''
    if check:
        try:
            if check(result):
                print(f'[PASS] {name}'); total_p += 1
            else:
                print(f'[FAIL] {name}: {repr(result[:100])}'); total_f += 1
        except Exception as e:
            print(f'[FAIL] {name}: {e}'); total_f += 1
    else:
        print(f'[OK]   {name}'); total_p += 1

test('1.CREATE TABLE', 'CREATE TABLE t1 (id INT, val INT);')
test('2.INSERT x3', 'INSERT INTO t1 VALUES (1, 100);')
test('2b', 'INSERT INTO t1 VALUES (2, 200);')
test('2c', 'INSERT INTO t1 VALUES (3, 300);')
test('3.SELECT *', 'SELECT * FROM t1;', True, lambda r: '1' in r and '200' in r)
test('4.CREATE INDEX', 'CREATE INDEX t1(val);')
test('5.SHOW INDEX', 'SHOW INDEX FROM t1;', True, lambda r: 't1' in r and 'val' in r)
test('6.UPDATE', 'UPDATE t1 SET val = 999 WHERE id = 1;')
test('7.SELECT after UPDATE', 'SELECT * FROM t1 WHERE val = 999;', True, lambda r: '999' in r)
test('8.DELETE', 'DELETE FROM t1 WHERE id = 2;')
test('9.SELECT after DELETE', 'SELECT * FROM t1;', True, lambda r: '1' in r and '2' not in r)
test('10.DROP TABLE', 'DROP TABLE t1;')
test('11.SHOW TABLES', 'SHOW TABLES;', True, lambda r: 't1' not in r)

print(f'\n=== {total_p} passed, {total_f} failed ===')
op = os.path.join(DB, 'output.txt')
if os.path.exists(op):
    fc = open(op).read().count('failure')
    print(f'output.txt: {fc} failures')
else:
    print('output.txt: not found')
s.close()
p.terminate(); p.wait()
