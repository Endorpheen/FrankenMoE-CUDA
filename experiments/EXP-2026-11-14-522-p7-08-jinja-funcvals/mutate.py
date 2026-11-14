#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-522-p7-08-jinja-funcvals'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('builtin values lose their marker', 'A[st[17] + f] = 0 - 1;', 'A[st[17] + f] = 0;'),
    ('range resolves to the raise_exception builtin', 'if filter_name_is(name, "range") {\n            return mk_builtin(A, F, st, 1);', 'if filter_name_is(name, "range") {\n            return mk_builtin(A, F, st, 2);'),
    ('range starts at one', 'var start: i64 = 0;\n        var stop: i64 = 0;', 'var start: i64 = 1;\n        var stop: i64 = 0;'),
    ('range step defaults to two', 'var step: i64 = 1;', 'var step: i64 = 2;'),
    ('ascending range includes the stop', 'while v < stop {', 'while v <= stop {'),
    ('descending range includes the stop', 'while v > stop {', 'while v >= stop {'),
    ('the step-zero error text differs', '"range() step argument must not be zero"', '"range() step is zero"'),
    ('raise_exception loses its prefix', '"Jinja Exception: "', '"Exception: "'),
    ('builtin calls ignore their arguments', 'let argc = al0c(A, st, id);', 'let argc: i64 = 1;'),
    ('raise_exception resolves to range', 'if filter_name_is(name, "raise_exception") {\n            return mk_builtin(A, F, st, 2);', 'if filter_name_is(name, "raise_exception") {\n            return mk_builtin(A, F, st, 1);'),
    ('single-argument range stops one late', 'stop = vi(A, st, argv[0]);', 'stop = vi(A, st, argv[0]) + 1;'),
    ('builtin identifiers are not resolved', 'if filter_name_is(name, "range") {\n            return mk_builtin(A, F, st, 1);', 'if false {\n            return mk_builtin(A, F, st, 1);'),
]






def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG522_RUNTIME_FUNC')]
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
