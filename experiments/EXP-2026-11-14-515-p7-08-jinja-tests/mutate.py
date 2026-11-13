#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-515-p7-08-jinja-tests'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('defined is inverted', 'filter_name_is(tname, "defined") {\n        if k != V_UNDEF {', 'filter_name_is(tname, "defined") {\n        if k == V_UNDEF {'),
    ('undefined is inverted', '} else if filter_name_is(tname, "undefined") {\n        if k == V_UNDEF {', '} else if filter_name_is(tname, "undefined") {\n        if k != V_UNDEF {'),
    ('none accepts strings', 'filter_name_is(tname, "none") {\n        if k == V_NONE {', 'filter_name_is(tname, "none") {\n        if k == V_NONE || k == V_STRING {'),
    ('number excludes floats', 'filter_name_is(tname, "number") {\n        if k == V_INT || k == V_FLOAT {', 'filter_name_is(tname, "number") {\n        if k == V_INT {'),
    ('odd and even are swapped', 'filter_name_is(tname, "odd") {\n            if rem != 0 {', 'filter_name_is(tname, "odd") {\n            if rem == 0 {'),
    ('divisibleby is inverted', 'if div != 0 && vi(A, st, v) % div == 0 {', 'if div != 0 && vi(A, st, v) % div != 0 {'),
    ('lower looks at lowercase bytes', 'filter_name_is(tname, "lower") {\n                if c >= 65 && c <= 90 {', 'filter_name_is(tname, "lower") {\n                if c >= 97 && c <= 122 {'),
    ('eq is inverted', 'res = same;', 'res = 1 - same;'),
    ('in ignores the result', 'res = r;', 'res = 0;'),
    ('negate is ignored', 'if aiv(A, st, id) != 0 {\n        res = 1 - res;', 'if false {\n        res = 1 - res;'),
    ('iterable excludes containers', 'if k == V_OBJECT || k == V_ARRAY || k == V_TUPLE || k == V_STRING || k == V_UNDEF {', 'if k == V_OBJECT || k == V_STRING || k == V_UNDEF {'),
    ('mapping accepts arrays', 'filter_name_is(tname, "mapping") {\n        if k == V_OBJECT {', 'filter_name_is(tname, "mapping") {\n        if k == V_ARRAY {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG515_RUNTIME_TESTS')]
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
