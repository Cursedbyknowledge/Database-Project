#!/usr/bin/env python3
import socket, time, subprocess, os
DB = '/tmp/upd_test'
subprocess.run(['rm', '-rf', DB])
p = subprocess.Popen(['build/bin/rmdb', DB], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)
s = socket.socket(); s.connect(('localhost', 8765)); time.sleep(0.2)

def do(sql, wait=0.3):
    s.sendall((sql + '\n').encode()); time.sleep(wait)
    s.settimeout(0.5)
    try: d = s.recv(8192); return d.decode()
    except socket.timeout: return '(timeout)'

do('CREATE TABLE t (a INT, b INT);')
do('INSERT INTO t VALUES (1, 10);')
do('INSERT INTO t VALUES (2, 20);')
print('1:', do('SELECT * FROM t;')[:200])
print('2. UPDATE...')
do('UPDATE t SET b = 99 WHERE a = 1;')
print('3. Server alive:', p.poll() is None)
print('4:', do('SELECT * FROM t;'))
print('5:', do('SELECT * FROM t WHERE a = 1;'))
s.close()
p.terminate(); p.wait()
