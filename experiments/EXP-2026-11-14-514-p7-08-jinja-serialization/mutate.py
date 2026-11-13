#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-514-p7-08-jinja-serialization'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('tojson accepts undefined', '    if filter_name_is(fname, "tojson") {\n        if vk(A, st, sval) == V_UNDEF {', '    if filter_name_is(fname, "tojson") {\n        if false {'),
    ('tojson is indented', '        let n = json_into(sval, 0, -1, ", ", ": ", A, B, F, st, out, base, E);\n\n        if n < 0 || st[1] != 0 {\n            return -1;\n        }\n        let res = mk_str(A, F, B, st, out[base..n]);', '        let n = json_into(sval, 0, 0, ", ", ": ", A, B, F, st, out, base, E);\n\n        if n < 0 || st[1] != 0 {\n            return -1;\n        }\n        let res = mk_str(A, F, B, st, out[base..n]);'),
    ('string on a string is not an identity', '        if k == V_STRING {\n            return sval;\n        }\n\n        if k == V_BOOL {', '        if k == V_STRING {\n            return mk_str(A, F, B, st, "");\n        }\n\n        if k == V_BOOL {'),
    ('bool False is rendered as True', '            if vi(A, st, sval) != 0 {\n                return mk_str(A, F, B, st, "True");', '            if vi(A, st, sval) == 0 {\n                return mk_str(A, F, B, st, "True");'),
    ('None is rendered lowercase', '        if k == V_NONE {\n            return mk_str(A, F, B, st, "None");', '        if k == V_NONE {\n            return mk_str(A, F, B, st, "none");'),
    ('undefined is rendered as x', '        if k == V_UNDEF {\n            return mk_str(A, F, B, st, "");', '        if k == V_UNDEF {\n            return mk_str(A, F, B, st, "x");'),
    ('array string is serialized as JSON', '        } else if k == V_ARRAY || k == V_TUPLE || k == V_OBJECT {\n            n = repr_into(A, B, F, st, sval, out, base, E);', '        } else if k == V_ARRAY || k == V_TUPLE || k == V_OBJECT {\n            n = json_into(sval, 0, -1, ", ", ": ", A, B, F, st, out, base, E);'),
    ('int truncates upward', '            return mk_int(A, F, st, F[sval] as i64);', '            return mk_int(A, F, st, (F[sval] + 1.0) as i64);'),
    ('int ignores the sign', '        if negi {\n            acc = 0 - acc;\n        }', '        if negi {\n            acc = acc;\n        }'),
    ('int ignores the default', '                return mk_int(A, F, st, vi(A, st, argv[0]));', '                return mk_int(A, F, st, 0);'),
    ('float ignores the exponent', '                if eany {', '                if false {'),
    ('float ignores the fraction', '            if i < src.len && src[i] == 46 {', '            if false {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG514_RUNTIME_SERIAL')]
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
