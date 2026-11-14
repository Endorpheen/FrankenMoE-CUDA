#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-521-p7-08-jinja-mutators'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('reverse keeps the original order', 'let res = arr_new(A, F, st);\n\n        if vk(A, st, sval) == V_TUPLE {\n            A[st[16] + res] = V_TUPLE;\n        }\n        var i: i64 = n - 1;', 'let res = arr_new(A, F, st);\n\n        if vk(A, st, sval) == V_TUPLE {\n            A[st[16] + res] = V_TUPLE;\n        }\n        var i: i64 = 0;'),
    ('list on an object returns the values', 'if filter_name_is(fname, "list") {\n        if vk(A, st, sval) == V_OBJECT {\n            let n = vkc(A, st, sval);\n            let res = arr_new(A, F, st);\n            var i: i64 = 0;\n\n            while i < n {\n                arr_push(A, st, res, A[pool(A, st) + vks(A, st, sval) + i]);', 'if filter_name_is(fname, "list") {\n        if vk(A, st, sval) == V_OBJECT {\n            let n = vkc(A, st, sval);\n            let res = arr_new(A, F, st);\n            var i: i64 = 0;\n\n            while i < n {\n                arr_push(A, st, res, A[pool(A, st) + vvs(A, st, sval) + i]);'),
    ('min and max are swapped', 'var op: i64 = 3;\n\n        if filter_name_is(fname, "max") {', 'var op: i64 = 2;\n\n        if filter_name_is(fname, "max") {'),
    ('max ignores the initial attribute', 'bestkey = attr_of(best, attr_val, undef, A, B, F, st);', 'bestkey = best;'),
    ('min/max compare the item instead of the attribute', 'key = attr_of(item, attr_val, undef, A, B, F, st);', 'key = item;'),
    ('append repeats the first element', 'arr_push(A, st, res, argv[0]);', 'arr_push(A, st, res, A[src_base]);'),
    ('pop ignores the index', 'return A[pool(A, st) + vcs(A, st, sval) + idx];', 'return A[pool(A, st) + vcs(A, st, sval)];'),
    ('pop does not resolve negative indices', 'if idx < 0 {\n            idx = n + idx;\n        }', 'if false {\n            idx = n + idx;\n        }'),
    ('pop rejects the last element', 'if idx < 0 || idx >= n {', 'if idx < 0 || idx >= n - 1 {'),
    ('join ignores the attribute', 'if join_attr {', 'if false {'),
    ('undefined sum answers a string', 'if filter_name_is(fname, "length") || filter_name_is(fname, "wordcount") ||\n                filter_name_is(fname, "sum") {', 'if filter_name_is(fname, "length") || filter_name_is(fname, "wordcount") {'),
    ('append drops the new element', 'arr_push(A, st, res, argv[0]);', 'arr_push(A, st, res, A[src_base + n - 1]);'),
]





def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG521_RUNTIME_MUT')]
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
