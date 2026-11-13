#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-517-p7-08-jinja-split'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('default delimiter becomes a comma', 'var delim: const u8[:] = " ";', 'var delim: const u8[:] = ",";'),
    ('split ignores maxsplit', 'var cur: i64 = 0;\n            var go: i64 = 1;\n\n            while maxsplit != 0 && go != 0 {', 'var cur: i64 = 0;\n            var go: i64 = 1;\n\n            while go != 0 {'),
    ('rsplit ignores maxsplit', 'var cur: i64 = src.len;\n            var go: i64 = 1;\n\n            while maxsplit != 0 && go != 0 {', 'var cur: i64 = src.len;\n            var go: i64 = 1;\n\n            while go != 0 {'),
    ('split drops the remainder', 'arr_push(A, st, arr, mk_str(A, F, B, st, src[cur..]));', 'arr_push(A, st, arr, mk_str(A, F, B, st, src[cur..cur]));'),
    ('rsplit is not reversed', 'while i + i < n {', 'while false {'),
    ('rsplit stops at the first match', 'if eq {\n                        pos = j;\n                    }\n                    j = j + 1;', 'if eq {\n                        pos = j;\n                        j = cur;\n                    }\n                    j = j + 1;'),
    ('the empty-separator error text differs', '"empty separator"', '"empty delimiter"'),
    ('rsplit drops the head', 'arr_push(A, st, arr, mk_str(A, F, B, st, src[0..cur]));', 'arr_push(A, st, arr, mk_str(A, F, B, st, src[0..0]));'),
    ('rsplit keeps the delimiter in the token', 'arr_push(A, st, arr, mk_str(A, F, B, st, src[pos + delim.len .. cur]));', 'arr_push(A, st, arr, mk_str(A, F, B, st, src[pos .. cur]));'),
    ('truncate is not answered for undefined', 'filter_name_is(fname, "last") || filter_name_is(fname, "truncate") {', 'filter_name_is(fname, "last") {'),
    ('split never stops after maxsplit', '                    maxsplit = maxsplit - 1;\n                }\n            }\n            arr_push(A, st, arr, mk_str(A, F, B, st, src[cur..]));', '                    maxsplit = maxsplit + 1;\n                }\n            }\n            arr_push(A, st, arr, mk_str(A, F, B, st, src[cur..]));'),
    ('split starts after the first token', 'arr_push(A, st, arr, mk_str(A, F, B, st, src[cur..pos]));', 'arr_push(A, st, arr, mk_str(A, F, B, st, src[cur..pos + 1]));'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG517_RUNTIME_SPLIT')]
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
