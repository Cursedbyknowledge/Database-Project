import socket
import time
import subprocess
import os
import sys

# Clean up
db_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build', 'index_test_db')
if os.path.exists(db_dir):
    import shutil
    shutil.rmtree(db_dir)

# Start server
build_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build')
proc = subprocess.Popen(['./bin/rmdb', 'index_test_db'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=build_dir)

# Wait for server to start
time.sleep(2)

# Test cases
tests = [
    ('create table warehouse (id int, name char(8));', None),
    ("insert into warehouse values (10 , 'qweruiop');", None),
    ("insert into warehouse values (534, 'asdfhjkl');", None),
    ("insert into warehouse values (100,'qwerghjk');", None),
    ("insert into warehouse values (500,'bgtyhnmj');", None),
    ('create index warehouse (id);', None),
    ('show index from warehouse;', '| warehouse | unique | (id) |'),
    ('create index warehouse (id,name);', None),
    ('show index from warehouse;', '| warehouse | unique | (id) |'),
    ('show index from warehouse;', '| warehouse | unique | (id,name) |'),
    ('drop index warehouse (id);', None),
    ('drop index warehouse (id,name);', None),
    ('show index from warehouse;', None),
    ('select * from warehouse where id = 10;', '| id | name |\n| 10 | qweruiop |'),
    ('select * from warehouse where id < 534 and id > 100;', '| id | name |\n| 500 | bgtyhnmj |'),
    ('create index warehouse(name);', None),
    ("select * from warehouse where name = 'qweruiop';", '| id | name |\n| 10 | qweruiop |'),
    ("select * from warehouse where name > 'qwerghjk';", '| id | name |'),
    ("select * from warehouse where name > 'aszdefgh' and name < 'qweraaaa';", '| id | name |'),
    ('drop index warehouse(name);', None),
    ('create index warehouse(id,name);', None),
    ("select * from warehouse where id = 100 and name = 'qwerghjk';", '| id | name |\n| 100 | qwerghjk |'),
    ("select * from warehouse where id < 600 and name > 'bztyhnmj';", '| id | name |'),
]

def send_sql(sock, sql):
    sock.send((sql + '\0').encode())

def recv_response(sock):
    data = b''
    while True:
        try:
            chunk = sock.recv(8192)
        except socket.timeout:
            break
        if not chunk:
            break
        data += chunk
        if b'\0' in data:
            break
    return data.replace(b'\0', b'').decode().strip()

try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect(('127.0.0.1', 8765))
    
    passed = 0
    failed = 0
    
    for sql, expected in tests:
        send_sql(sock, sql)
        resp = recv_response(sock)
        print(f'SQL: {sql}')
        print(f'RESP: {repr(resp)}')
        if expected is not None:
            if expected in resp:
                print(f'  PASS')
                passed += 1
            else:
                print(f'  EXPECTED: {repr(expected)}')
                print(f'  FAIL')
                failed += 1
        else:
            print(f'  (no expected)')
        print()
    
    print(f'=' * 50)
    print(f'Passed: {passed}, Failed: {failed}')
    
    send_sql(sock, 'exit;')
    time.sleep(0.5)
    sock.close()
except Exception as e:
    print(f'Error: {e}')
    import traceback
    traceback.print_exc()

proc.terminate()
proc.wait()
print('Done')