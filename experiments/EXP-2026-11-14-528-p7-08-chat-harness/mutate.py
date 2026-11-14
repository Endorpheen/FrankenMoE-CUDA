#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-528-p7-08-chat-harness'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('loop.index starts at zero', 'lv[0] = mk_int(A, F, st, j + 1);', 'lv[0] = mk_int(A, F, st, j);'),
    ('loop.length is one short', 'lv[6] = mk_int(A, F, st, kn);', 'lv[6] = mk_int(A, F, st, kn - 1);'),
    ('object keys all match', 'if eqv(A, B, F, st, k, key) {', 'if true {'),
    ('the default boolean check is inverted', 'if v_bool(A, F, st, sval) == 0 {', 'if v_bool(A, F, st, sval) == 1 {'),
    ('every if body runs', 'if v_bool(A, F, st, test) != 0 {', 'if v_bool(A, F, st, test) == 0 {'),
    ('array length is one too long', 'return mk_int(A, F, st, vcc(A, st, sval));', 'return mk_int(A, F, st, vcc(A, st, sval) + 1);'),
    ('computed members are treated as static', 'if aiv(A, st, id) != 0 {\n        prop = eval_node(ac1(A, st, id)', 'if aiv(A, st, id) == 0 {\n        prop = eval_node(ac1(A, st, id)'),
    ('object member lookup always misses', 'let found = obj_lookup(A, B, F, st, obj, prop);', 'let found = 0 - 1;'),
    ('statement order is reversed', 'let child = A[apool(A, st) + start + i];', 'let child = A[apool(A, st) + start + count - 1 - i];'),
    ('the last array element is rejected', 'if idx >= 0 && idx < n {', 'if idx >= 0 && idx < n - 1 {'),
    ('static member names are empty', 'prop = mk_str(A, F, B, st, astr(B, A, st, pn));', 'prop = mk_str(A, F, B, st, "");'),
    ('empty strings count as missing', 'if vk(A, st, sval) == V_UNDEF || vk(A, st, sval) == V_NONE {', 'if vk(A, st, sval) == V_UNDEF || vk(A, st, sval) == V_NONE || vk(A, st, sval) == V_STRING {'),
]












def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG528_RUNTIME_CHAT')]
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
