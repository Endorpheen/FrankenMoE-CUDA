#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-513-p7-08-jinja-filters-more'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('title capitalizes only the first word', '            if ws_byte(c) {\n                cap = 1;', '            if ws_byte(c) {\n                cap = 0;'),
    ('title uppercases the rest of the string', '            } else {\n                c = ascii_lower(c);\n            }\n            out[base + i] = c;', '            } else {\n                c = ascii_upper(c);\n            }\n            out[base + i] = c;'),
    ('lstrip trims the right side instead', '        if filter_name_is(fname, "lstrip") {', '        if false {'),
    ('rstrip skips one byte too many', '                if hit {\n                    b = b - 1;', '                if hit {\n                    b = b - 2;'),
    ('lstrip/rstrip ignore the chars argument', '        if argc > 0 && vk(A, st, argv[0]) != V_UNDEF {\n            let cbase = st[7];', '        if false {\n            let cbase = st[7];'),
    ('first returns the last element', '        if filter_name_is(fname, "first") {\n            return A[pool(A, st) + vcs(A, st, sval)];\n        }', '        if filter_name_is(fname, "first") {\n            return A[pool(A, st) + vcs(A, st, sval) + n - 1];\n        }'),
    ('last returns the first element', '        return A[pool(A, st) + vcs(A, st, sval) + n - 1];', '        return A[pool(A, st) + vcs(A, st, sval)];'),
    ('first on an empty container is not undefined', '        if n == 0 {\n            return mk_undef(A, F, st);\n        }', '        if false {\n            return mk_undef(A, F, st);\n        }'),
    ('join drops the last element', '        while i < n {\n            let item = A[pool(A, st) + vcs(A, st, sval) + i];', '        while i + 1 < n {\n            let item = A[pool(A, st) + vcs(A, st, sval) + i];'),
    ('join ignores the delimiter', '            if i + 1 < n && argc > 0 && vk(A, st, argv[0]) != V_UNDEF {', '            if false {'),
    ('undefined length is one', '            if filter_name_is(fname, "length") || filter_name_is(fname, "wordcount") {\n                return mk_int(A, F, st, 0);', '            if filter_name_is(fname, "length") || filter_name_is(fname, "wordcount") {\n                return mk_int(A, F, st, 1);'),
    ('undefined first returns an empty string', '            if filter_name_is(fname, "first") || filter_name_is(fname, "last") {\n                return mk_undef(A, F, st);', '            if filter_name_is(fname, "first") || filter_name_is(fname, "last") {\n                return mk_str(A, F, B, st, "");'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG513_RUNTIME_FILTERS')]
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
