#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-526-p7-08-jinja-scalarmethods'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('int abs keeps the sign', 'if v < 0 {\n                v = 0 - v;\n            }', 'if false {\n                v = 0 - v;\n            }'),
    ('float abs keeps the sign', 'if f < 0.0 {\n                f = 0.0 - f;\n            }', 'if false {\n                f = 0.0 - f;\n            }'),
    ('abs accepts booleans', 'if vk(A, st, sval) == V_INT {\n            var v = vi(A, st, sval);', 'if vk(A, st, sval) == V_INT || vk(A, st, sval) == V_BOOL {\n            var v = vi(A, st, sval);'),
    ('booleans gain an abs method', 'if kind == V_BOOL {\n        return filter_name_is(n, "int") || filter_name_is(n, "float") ||', 'if kind == V_BOOL {\n        return filter_name_is(n, "abs") || filter_name_is(n, "int") || filter_name_is(n, "float") ||'),
    ('floats lose the abs method', 'if kind == V_INT || kind == V_FLOAT {\n        return filter_name_is(n, "abs") || filter_name_is(n, "int") ||', 'if kind == V_INT || kind == V_FLOAT {\n        return filter_name_is(n, "int") ||'),
    ('booleans lose the tojson method', 'return filter_name_is(n, "int") || filter_name_is(n, "float") ||\n            filter_name_is(n, "string") || filter_name_is(n, "safe") ||\n            filter_name_is(n, "tojson");', 'return filter_name_is(n, "int") || filter_name_is(n, "float") ||\n            filter_name_is(n, "string") || filter_name_is(n, "safe");'),
    ('booleans lose the string method', 'if kind == V_BOOL {\n        return filter_name_is(n, "int") || filter_name_is(n, "float") ||\n            filter_name_is(n, "string") || filter_name_is(n, "safe") ||', 'if kind == V_BOOL {\n        return filter_name_is(n, "int") || filter_name_is(n, "float") ||\n            filter_name_is(n, "safe") ||'),
    ('a missing scalar method loses its hint', 'if vk(A, st, prop) == V_STRING {\n        return mk_undef_hint(A, F, B, st, vstr(B, A, st, prop));', 'if false {\n        return mk_undef_hint(A, F, B, st, vstr(B, A, st, prop));'),
    ('float abs returns the input', 'return mk_float(A, F, st, f);', 'return mk_float(A, F, st, F[sval]);'),
    ('int abs returns a float', 'return mk_int(A, F, st, v);\n        }\n\n        if vk(A, st, sval) == V_FLOAT {', 'return mk_float(A, F, st, v as f64);\n        }\n\n        if vk(A, st, sval) == V_FLOAT {'),
    ('scalars have no methods', 'if kind == V_INT || kind == V_FLOAT {\n        return filter_name_is(n, "abs") || filter_name_is(n, "int") ||', 'if false {\n        return filter_name_is(n, "abs") || filter_name_is(n, "int") ||'),
    ('abs accepts only floats', 'if vk(A, st, sval) == V_INT {\n            var v = vi(A, st, sval);', 'if false {\n            var v = vi(A, st, sval);'),
]










def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG526_RUNTIME_SCALAR')]
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
