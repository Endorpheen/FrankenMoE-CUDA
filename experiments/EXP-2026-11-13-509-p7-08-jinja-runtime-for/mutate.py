#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-13-509-p7-08-jinja-runtime-for'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('object iteration binds tuples instead of keys', 'key_mode = 1;', 'key_mode = 0;'),
    ('object iteration runs zero times', 'count = vvc(A, st, iter);', 'count = 0;'),
    ('select filter is ignored', 'take = v_bool(A, F, st, tv);', 'take = 1;'),
    ('break does not stop the loop', 'j = kn;', 'j = j + 1;'),
    ('continue stops the loop', """        } else if st[8] == 2 {
            st[8] = 0;
            scope_pop(st);
            j = j + 1;""", """        } else if st[8] == 2 {
            st[8] = 0;
            scope_pop(st);
            j = kn;"""),
    ('loop.index is zero based', 'lv[0] = mk_int(A, F, st, j + 1);', 'lv[0] = mk_int(A, F, st, j);'),
    ('loop.revindex is one off', 'lv[2] = mk_int(A, F, st, kn - j);', 'lv[2] = mk_int(A, F, st, kn - j - 1);'),
    ('loop.first marks the last item', 'if j == 0 {\n            first = 1;', 'if j == kn - 1 {\n            first = 1;'),
    ('loop.length is the position', 'lv[6] = mk_int(A, F, st, kn);', 'lv[6] = mk_int(A, F, st, j);'),
    ('loop.previtem is always undefined', 'lv[7] = item_at(A, F, st, iter, key_mode, vi(A, st, vchild(A, st, kept, j - 1)));', 'lv[7] = mk_undef_hint(A, F, B, st, "previtem");'),
    ('tuple unpack size is not checked', 'if want != 2 {', 'if false {'),
    ('object loop variable binds the value', 'return ctx_set(name, key, cnoff, cnlen, cval, B, A, F, st, E);', 'return ctx_set(name, val, cnoff, cnlen, cval, B, A, F, st, E);'),
    ('else block never runs', 'if completed == 0 {', 'if false {'),
    ('else block always runs', 'if completed == 0 {', 'if completed >= 0 {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG509_RUNTIME_FOR')]
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
