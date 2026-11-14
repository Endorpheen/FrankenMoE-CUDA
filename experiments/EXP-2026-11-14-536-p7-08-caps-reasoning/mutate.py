#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-536-p7-08-caps-reasoning'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('rendered values lose the used bit', 'A[st[27] + v] = A[st[27] + v] | 1;', 'A[st[27] + v] = A[st[27] + v] | 0;'),
    ('strings render as nothing', 'return out_bytes(out, st, vstr(B, A, st, v));', 'return 0;'),
    ('identifiers resolve to undefined', 'return cval[i];', 'return mk_undef(A, F, st);'),
    ('the if condition is inverted', 'if v_bool(A, F, st, test) != 0 {', 'if v_bool(A, F, st, test) == 0 {'),
    ('object lookup always misses', 'let found = obj_lookup(A, B, F, st, obj, prop);', 'let found = 0 - 1;'),
    ('static member names are empty', 'prop = mk_str(A, F, B, st, astr(B, A, st, pn));', 'prop = mk_str(A, F, B, st, "");'),
    ('the loop iterates one item', 'count = vcc(A, st, iter);', 'count = 1;'),
    ('the loop variable is the whole array', 'bind_loop_item(loopvar, vchild(A, st, iter, i), 0,', 'bind_loop_item(loopvar, iter, 0,'),
    ('the var mask export returns zero', 'export fn chat_probe_mask(A: const i64[:], st: const i64[:], id: i64) -> i64 {\n    return A[st[27] + id];', 'export fn chat_probe_mask(A: const i64[:], st: const i64[:], id: i64) -> i64 {\n    return 0;'),
    ('the var mask export shifts the bit away', 'export fn chat_probe_mask(A: const i64[:], st: const i64[:], id: i64) -> i64 {\n    return A[st[27] + id];', 'export fn chat_probe_mask(A: const i64[:], st: const i64[:], id: i64) -> i64 {\n    return A[st[27] + id] >> 1;'),
    ('the var mask export returns the id', 'export fn chat_probe_mask(A: const i64[:], st: const i64[:], id: i64) -> i64 {\n    return A[st[27] + id];', 'export fn chat_probe_mask(A: const i64[:], st: const i64[:], id: i64) -> i64 {\n    return A[st[27] + id] & 0;'),
    ('appends mark the first value only', 'A[st[27] + v] = A[st[27] + v] | 1;', 'A[st[27] + 0] = A[st[27] + 0] | 1;'),
]





















def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG536_RUNTIME_REASON')]
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
