#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-519-p7-08-jinja-containers'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('keys returns values', 'arr_push(A, st, arr, A[pool(A, st) + vks(A, st, sval) + i]);', 'arr_push(A, st, arr, A[pool(A, st) + vvs(A, st, sval) + i]);'),
    ('values returns keys', 'arr_push(A, st, arr, A[pool(A, st) + vvs(A, st, sval) + i]);', 'arr_push(A, st, arr, A[pool(A, st) + vks(A, st, sval) + i]);'),
    ('items repeats the key as the value', 'A[st[26] + tb + 1] = A[pool(A, st) + vvs(A, st, sval) + i1];', 'A[st[26] + tb + 1] = A[pool(A, st) + vks(A, st, sval) + i1];'),
    ('items tuples hold one element', 'A[st[21] + t] = 2;', 'A[st[21] + t] = 1;'),
    ('get ignores the default', 'if found >= 0 {\n            return found;\n        }\n\n        if argc > 1 {', 'if found >= 0 {\n            return found;\n        }\n\n        if false {'),
    ('get ignores the hit', 'let found = obj_lookup(A, B, F, st, sval, argv[0]);\n\n        if found >= 0 {', 'let found = obj_lookup(A, B, F, st, sval, argv[0]);\n\n        if false {'),
    ('sort is descending by default', 'var op: i64 = 3;', 'var op: i64 = 2;'),
    ('sort ignores reverse', 'if argc > 0 && vk(A, st, argv[0]) != V_UNDEF && v_bool(A, F, st, argv[0]) != 0 {\n                op = 2;', 'if false {\n                op = 2;'),
    ('sort places the key at the wrong slot', 'A[base + j + 1] = key;', 'A[base + i] = key;'),
    ('unique keeps duplicates', 'if dup == 0 {', 'if true {'),
    ('unique drops the first occurrence', 'if r != 0 {\n                    dup = 1;', 'if r == 0 {\n                    dup = 1;'),
    ('items loses the tuple reference', 'A[abase + i1] = t;', 'A[abase + i1] = 0;'),
]



def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG519_RUNTIME_CONT')]
    if line:
        return r.returncode, line[-1]
    # No gate line: either the build stopped (goose, gcc or link) or the binary
    # died before reporting. Both are detected differences, so both count as
    # caught; only the detail differs.
    tail = [x for x in (r.stdout + '\n' + r.stderr).splitlines() if x.strip()]
    detail = tail[-1][:140] if tail else 'empty log'
    if not any(x.startswith('build mode=') for x in r.stdout.splitlines()):
        return 1, 'build stopped: ' + detail
    return max(r.returncode, 1), 'binary produced no gate line: ' + detail


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
try:
    for label, old, new in M[lo:hi]:
        if old not in ORIG:
            print('[skip]', label)
            continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, line = run()
        if rc is None:
            print('[build-fail]', label)
            continue
        ok = rc != 0
        caught += ok
        print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
finally:
    SRC.write_text(ORIG)
    rc, line = run()
    print(f'baseline after restore: rc={rc} {line}')
print(f'mutations {caught}/{hi-lo} caught (range {lo}:{hi})')
