#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-527-p7-08-jinja-default'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('undefined is not treated as missing', 'if vk(A, st, sval) == V_UNDEF || vk(A, st, sval) == V_NONE {', 'if vk(A, st, sval) == V_NONE {'),
    ('none is not treated as missing', 'if vk(A, st, sval) == V_UNDEF || vk(A, st, sval) == V_NONE {', 'if vk(A, st, sval) == V_UNDEF {'),
    ('the boolean check is inverted', 'if v_bool(A, F, st, sval) == 0 {', 'if v_bool(A, F, st, sval) == 1 {'),
    ('the boolean keyword is ignored', 'if has_attr && filter_name_is(kwattr, "boolean") {\n            check = v_bool(A, F, st, kwval);', 'if false {\n            check = v_bool(A, F, st, kwval);'),
    ('the positional boolean is ignored', 'if has_attr && filter_name_is(kwattr, "boolean") {\n            check = v_bool(A, F, st, kwval);\n        } else if argc > 1 && vk(A, st, argv[1]) != V_UNDEF {', 'if has_attr && filter_name_is(kwattr, "boolean") {\n            check = v_bool(A, F, st, kwval);\n        } else if false {'),
    ('the default is never used', 'if noval {\n            return argv[0];', 'if false {\n            return argv[0];'),
    ('missing values return the input', 'if noval {\n            return argv[0];\n        }\n        return sval;', 'if noval {\n            return sval;\n        }\n        return sval;'),
    ('strings lose default', '&& vk(A, st, sval) != V_OBJECT', '&& vk(A, st, sval) != V_STRING'),
    ('the string method loses default', 'filter_name_is(n, "default") || filter_name_is(n, "upper")', 'filter_name_is(n, "upper")'),
    ('the bool method loses default', 'filter_name_is(n, "default") || filter_name_is(n, "int")', 'filter_name_is(n, "int")'),
    ('the number method loses default', 'filter_name_is(n, "default") || filter_name_is(n, "abs")', 'filter_name_is(n, "abs")'),
    ('the array method loses default', 'filter_name_is(n, "default") || filter_name_is(n, "first")', 'filter_name_is(n, "first")'),
]











def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG527_RUNTIME_DEF')]
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
