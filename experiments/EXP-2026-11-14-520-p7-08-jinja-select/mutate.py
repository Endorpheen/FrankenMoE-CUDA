#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-520-p7-08-jinja-select'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('map keeps the item instead of the attribute', 'arr_push(A, st, res, attr_of(item, kwval, undef, A, B, F, st));', 'arr_push(A, st, res, item);'),
    ('map never returns a tuple', 'if vk(A, st, sval) == V_TUPLE {\n            A[st[16] + res] = V_TUPLE;\n        }\n        let undef = mk_undef(A, F, st);', 'if false {\n            A[st[16] + res] = V_TUPLE;\n        }\n        let undef = mk_undef(A, F, st);'),
    ('reject behaves like select', 'if filter_name_is(fname, "reject") {', 'if filter_name_is(fname, "select") {'),
    ('select keeps everything', 'if sel != 0 {', 'if true {'),
    ('a missing attribute keeps the item', 'return defid;\n    }\n\n    if ik == V_ARRAY', 'return item;\n    }\n\n    if ik == V_ARRAY'),
    ('attribute index is bounded by one less', 'if idx >= 0 && idx < vcc(A, st, item) {', 'if idx >= 0 && idx < vcc(A, st, item) - 1 {'),
    ('dictsort always sorts by value', 'if by_value {', 'if true {'),
    ('dictsort ignores reverse', 'if argc > 2 && vk(A, st, argv[2]) != V_UNDEF && v_bool(A, F, st, argv[2]) != 0 {', 'if false {'),
    ('dictsort shifts values out of step', 'A[st[26] + vb + j + 1] = A[st[26] + vb + j];', 'A[st[26] + vb + j + 1] = A[st[26] + kb + j];'),
    ('dictsort by value reads the key', 'cur = val;', 'cur = key;'),
    ('dictsort drops one pair from the count', 'A[st[23] + obj] = n;', 'A[st[23] + obj] = n - 1;'),
    ('dictsort compares with the wrong order', 'var op: i64 = 3;\n\n        if argc > 2 && vk(A, st, argv[2]) != V_UNDEF', 'var op: i64 = 2;\n\n        if argc > 2 && vk(A, st, argv[2]) != V_UNDEF'),
]




def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG520_RUNTIME_SEL')]
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
