#!/usr/bin/env python3
"""RMDB full integration test - independent connections per SQL"""
import socket, time, subprocess, os, sys

HOST, PORT = 'localhost', 8765
BUILD_DIR = '/home/zyc/db2026/build'
DB_DIR = '/tmp/rmdb_test_final'

def send_sql(sql, expect=False):
    s = socket.socket(); s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    s.connect((HOST, PORT))
    time.sleep(0.3)
    s.sendall((sql + '\n').encode())
    time.sleep(0.5)  # wait for server to read
    if expect:
        s.shutdown(socket.SHUT_WR)  # signal EOF only when expecting response
        time.sleep(0.5)  # wait for server to process
        s.settimeout(1.0)
        chunks = []
        try:
            while True:
                d = s.recv(4096)
                if not d: break
                chunks.append(d)
        except socket.timeout: pass
        s.close()
        result = b''.join(chunks).decode('utf-8', errors='replace')
        time.sleep(0.3)
        return result
    s.close()
    time.sleep(0.3)
    return ""

def main():
    subprocess.run(['pkill', '-9', 'rmdb'], capture_output=True)
    time.sleep(0.3)
    subprocess.run(['rm', '-rf', DB_DIR], capture_output=True)
    server = subprocess.Popen([f'{BUILD_DIR}/bin/rmdb', DB_DIR],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.5)
    
    p = 0; f_ = 0
    def ok(msg): nonlocal p; print(f'[PASS] {msg}'); p += 1
    def fail(msg): nonlocal f_; print(f'[FAIL] {msg}'); f_ += 1
    
    try:
        ok("1. CREATE TABLE"); send_sql("CREATE TABLE t1 (id INT, val INT);")
        ok("2. INSERT x3")
        send_sql("INSERT INTO t1 VALUES (1, 100);")
        send_sql("INSERT INTO t1 VALUES (2, 200);")
        send_sql("INSERT INTO t1 VALUES (3, 300);")
        
        r = send_sql("SELECT * FROM t1;", True)
        (ok if ' 1 |' in r and ' 100 |' in r else fail)("3. SELECT *")
        
        ok("4. CREATE INDEX"); send_sql("CREATE INDEX t1(val);")
        
        r = send_sql("SHOW INDEX FROM t1;", True)
        (ok if '| t1 | unique | (val) |' in r else fail)("5. SHOW INDEX")
        
        r = send_sql("SELECT * FROM t1 WHERE val = 200;", True)
        (ok if '2' in r and '200' in r else fail)("6. Index scan val=200")
        
        ok("7. UPDATE"); send_sql("UPDATE t1 SET val = 999 WHERE id = 1;")
        
        r = send_sql("SELECT * FROM t1 WHERE val = 100;", True)
        (ok if '100' not in r or '0' in r.split('Total')[-1] else fail)("8. val=100 empty after UPDATE")
        
        r = send_sql("SELECT * FROM t1 WHERE val = 999;", True)
        (ok if ' 1 |' in r and ' 999 |' in r else fail)("9. val=999 after UPDATE")
        
        ok("10. DELETE"); send_sql("DELETE FROM t1 WHERE id = 2;")
        
        r = send_sql("SELECT * FROM t1;", True)
        (ok if ' 1 |' in r and ' 2 |' not in r and '200' not in r else fail)("11. After DELETE")
        
        r = send_sql("SELECT * FROM t1 WHERE val = 200;", True)
        (ok if '200' not in r or '0' in r.split('Total')[-1] else fail)("12. val=200 empty after DELETE")
        
        ok("13. DROP INDEX"); send_sql("DROP INDEX t1(val);")
        r = send_sql("SHOW INDEX FROM t1;", True)
        (ok if 'val' not in r else fail)("14. SHOW INDEX after drop")
        
        ok("15. DROP TABLE"); send_sql("DROP TABLE t1;")
        r = send_sql("SHOW TABLES;", True)
        (ok if 't1' not in r else fail)("16. SHOW TABLES after drop")
        
        # Check output.txt
        op = os.path.join(DB_DIR, 'output.txt')
        if os.path.exists(op):
            c = open(op).read()
            fc = c.count('failure')
            (ok if fc == 0 else fail)(f"output.txt ({fc} failures)")
        else:
            fail("output.txt not found")
        
        print(f'\n=== {p} passed, {f_} failed ===')
        sys.exit(0 if f_ == 0 else 1)
    except Exception as e:
        print(f'\n[ERROR] {e}')
        import traceback; traceback.print_exc()
        sys.exit(1)
    finally:
        server.terminate(); server.wait()

if __name__ == '__main__': main()
