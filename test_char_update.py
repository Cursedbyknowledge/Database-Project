#!/usr/bin/env python3
import socket, time, subprocess, os, sys

DB = '/tmp/upd_char'
subprocess.run(['rm', '-rf', DB])
p = subprocess.Popen(['build/bin/rmdb', DB], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)
s = socket.socket(); s.connect(('localhost', 8765)); time.sleep(0.2)

def do(sql, wait=0.3):
    s.sendall((sql + '\n').encode()); time.sleep(wait)
    s.settimeout(2.0)
    try: d = s.recv(8192); return d.decode()
    except socket.timeout: return '(timeout)'

do("CREATE TABLE t (name CHAR(10), val INT);")
do("INSERT INTO t VALUES ('aaa', 1);")
do("INSERT INTO t VALUES ('bbb', 2);")
print("SELECT:", do("SELECT * FROM t;")[:200])
print("UPDATE val=99 WHERE name='aaa'...")
do("UPDATE t SET val = 99 WHERE name = 'aaa';")
print("Server alive:", p.poll() is None)
print("SELECT after:", do("SELECT * FROM t;")[:200])
s.close(); p.terminate(); p.wait()
