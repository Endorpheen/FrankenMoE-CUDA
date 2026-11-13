#!/usr/bin/env python3
"""Local gate for the state-write adapter: build the bridge with the generated module and run it."""
from pathlib import Path
import subprocess, sys

E = Path(__file__).resolve().parent
B = E / 'build'
B.mkdir(exist_ok=True)

def run(cmd, **kw):
    q = subprocess.run(cmd, capture_output=True, text=True, **kw)
    print('---', ' '.join(str(c) for c in cmd), 'rc=', q.returncode)
    if q.stdout.strip():
        print(q.stdout.strip()[:2000])
    if q.returncode:
        print(q.stderr.strip()[:2000])
    return q

q = run(['gcc', '-O2', '-std=c11', '-c', str(E/'source/bridge.c'), '-o', str(B/'bridge.o'),
         '-I', str(E/'source')])
assert q.returncode == 0
q = run(['g++', '-O2', '-std=c++17', str(E/'tests/adaptor.cpp'), str(B/'bridge.o'), '-o', str(B/'adaptor')])
assert q.returncode == 0
q = run([str(B/'adaptor')])
assert q.returncode == 0 and 'failures=0' in q.stdout
print('EXP347_LOCAL_GATES_PASS')
