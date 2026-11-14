#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-545-p7-08-real-templates'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('member assignment writes the key', '                A[st[26] + vvs(A, st, objv) + i] = rhs;', '                A[st[26] + vvs(A, st, objv) + i] = k;'),
    ('member assignment matches any key', '            if eqv(A, B, F, st, k, key) {\n                A[st[26] + vvs(A, st, objv) + i] = rhs;', '            if true {\n                A[st[26] + vvs(A, st, objv) + i] = rhs;'),
    ('member assignment never finds the key', '        let n = vkc(A, st, objv);\n        var i: i64 = 0;', '        let n = vkc(A, st, objv);\n        var i: i64 = n;'),
    ('a new key stores the value as its key', '        obj_key(A, st, objv, key);', '        obj_key(A, st, objv, rhs);'),
    ('a new value stores the key', '        obj_val(A, st, objv, rhs);', '        obj_val(A, st, objv, key);'),
    ('the member branch is skipped', '    if akind == K_MEMBER {', '    if false {'),
    ('the static member name is empty', '            key = mk_str(A, F, B, st, astr(B, A, st, ac1(A, st, assignee)));', '            key = mk_str(A, F, B, st, "");'),
    ('computed and static members are swapped', '        if aiv(A, st, assignee) != 0 {\n            key = eval_node(ac1(A, st, assignee)', '        if aiv(A, st, assignee) == 0 {\n            key = eval_node(ac1(A, st, assignee)'),
    ('the length filter counts one more', '        if vk(A, st, sval) == V_ARRAY || vk(A, st, sval) == V_TUPLE {\n            return mk_int(A, F, st, vcc(A, st, sval));', '        if vk(A, st, sval) == V_ARRAY || vk(A, st, sval) == V_TUPLE {\n            return mk_int(A, F, st, vcc(A, st, sval) + 1);'),
    ('the if body is inverted', 'if v_bool(A, F, st, test) != 0 {', 'if v_bool(A, F, st, test) == 0 {'),
    ('the loop iterates one item', 'count = vcc(A, st, iter);', 'count = 1;'),
    ('object member lookup always misses', 'let found = obj_lookup(A, B, F, st, obj, prop);', 'let found = 0 - 1;'),
]






























def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG545_RUNTIME_REAL')]
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
