#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-535-p7-08-caps-parallel'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('the tool argument reads the calls array', 'let fnobj = obj_get(A, B, st, c0, "function");', 'let fnobj = obj_get(A, B, st, calls, "function");'),
    ('the tool argument key is misspelled', 'let arg = obj_get(A, B, st, args, "arg");', 'let arg = obj_get(A, B, st, args, "args");'),
    ('the tool argument reports unused', 'return A[st[27] + arg];', 'return 0;'),
    ('the tool argument reads the message', 'let c0 = arr_at(A, st, calls, 0);', 'let c0 = m1;'),
    ('the tool argument reads the second call', 'let c0 = arr_at(A, st, calls, 0);', 'let c0 = arr_at(A, st, calls, 1);'),
    ('the second call reads the first', 'let c1 = arr_at(A, st, calls, 1);', 'let c1 = arr_at(A, st, calls, 0);'),
    ('the second call reports unused', 'return A[st[27] + fnobj];', 'return 0;'),
    ('the second function key is misspelled', 'let fnobj = obj_get(A, B, st, c1, "function");', 'let fnobj = obj_get(A, B, st, c1, "functions");'),
    ('the second tool_calls key is misspelled', 'let calls = obj_get(A, B, st, m1, "tool_calls");\n\n    if calls < 0 {\n        return 0 - 1;\n    }\n    let c1 = arr_at(A, st, calls, 1);', 'let calls = obj_get(A, B, st, m1, "tool_call");\n\n    if calls < 0 {\n        return 0 - 1;\n    }\n    let c1 = arr_at(A, st, calls, 1);'),
    ('the arguments key is misspelled', 'let args = obj_get(A, B, st, fnobj, "arguments");', 'let args = obj_get(A, B, st, fnobj, "arg");'),
    ('member access stops marking the container', 'A[st[27] + obj] = A[st[27] + obj] | 1;', 'A[st[27] + obj] = A[st[27] + obj] | 0;'),
    ('member access marks the property instead', 'A[st[27] + obj] = A[st[27] + obj] | 1;', 'A[st[27] + prop] = A[st[27] + prop] | 1;'),
]




















def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG535_RUNTIME_PAR')]
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
