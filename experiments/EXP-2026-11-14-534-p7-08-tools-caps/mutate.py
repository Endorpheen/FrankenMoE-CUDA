#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-534-p7-08-tools-caps'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('the object walk matches any key', 'if bytes_is(B[koff .. koff + klen], key) {', 'if true {'),
    ('the object walk returns the key id', 'return A[pool(A, st) + vvs(A, st, obj) + i];', 'return k;'),
    ('the tool name key is read as description', 'let name = obj_get(A, B, st, fnobj, "name");', 'let name = obj_get(A, B, st, fnobj, "description");'),
    ('the key length is read as an offset', 'let klen = A[st[19] + k];', 'let klen = A[st[18] + k];'),
    ('the tool calls key is misspelled', 'let calls = obj_get(A, B, st, m1, "tool_calls");', 'let calls = obj_get(A, B, st, m1, "tool_call");'),
    ('array indexing returns the first child', 'return A[pool(A, st) + vcs(A, st, arr) + i];', 'return A[pool(A, st) + vcs(A, st, arr)];'),
    ('array indexing rejects containers', 'if vk(A, st, arr) != V_ARRAY && vk(A, st, arr) != V_TUPLE {', 'if true {'),
    ('the tool name reads the second tool', 'let t0 = arr_at(A, st, tools, 0);', 'let t0 = arr_at(A, st, tools, 1);'),
    ('the tool name reports unused', 'return A[st[27] + name];', 'return 0;'),
    ('the tool calls read the first message', 'let m1 = arr_at(A, st, msgs, 1);', 'let m1 = arr_at(A, st, msgs, 0);'),
    ('the tool calls report the message mask', 'return A[st[27] + calls];', 'return A[st[27] + m1];'),
    ('the tool function key is misspelled', 'let fnobj = obj_get(A, B, st, t0, "function");', 'let fnobj = obj_get(A, B, st, t0, "functions");'),
]



















def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG534_RUNTIME_TOOLS')]
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
