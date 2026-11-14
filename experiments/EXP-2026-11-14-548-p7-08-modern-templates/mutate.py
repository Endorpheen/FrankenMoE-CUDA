#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-548-p7-08-modern-templates'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('selectattr is not accepted', 'filter_name_is(fname, "selectattr") || filter_name_is(fname, "rejectattr")) &&', 'filter_name_is(fname, "select") || filter_name_is(fname, "rejectattr")) &&'),
    ('rejectattr behaves like selectattr', '            if filter_name_is(fname, "reject") || filter_name_is(fname, "rejectattr") {', '            if filter_name_is(fname, "reject") {'),
    ('the array method table loses selectattr', '            filter_name_is(n, "reject") || filter_name_is(n, "selectattr") ||', '            filter_name_is(n, "reject") ||'),
    ('tojson ignores its indent', '        if ind_arg >= 0 && vk(A, st, ind_arg) == V_INT {', '        if false {'),
    ('tojson keeps the compact separators', '            jisep = ",";', '            jisep = ", ";'),
    ('tojson drops the indent value', '            jindent = vi(A, st, ind_arg);', '            jindent = 0 - 1;'),
    ('tojson ignores the positional indent', '        } else if argc > 1 {\n            ind_arg = argv[1];', '        } else if false {\n            ind_arg = argv[1];'),
    ('tojson ignores the indent keyword', '        if has_attr && filter_name_is(kwattr, "indent") {\n            ind_arg = kwval;', '        if false {\n            ind_arg = kwval;'),
    ('tojson key separator loses its space', '        var jksep: const u8[:] = ": ";', '        var jksep: const u8[:] = ":";'),
    ('tojson indent is one deeper', '            jindent = vi(A, st, ind_arg);', '            jindent = vi(A, st, ind_arg) + 1;'),
    ('tojson never uses the keyword value', '            ind_arg = kwval;', '            ind_arg = -1;'),
    ('tojson reads the first positional as the indent', '        } else if argc > 1 {\n            ind_arg = argv[1];', '        } else if argc > 0 {\n            ind_arg = argv[0];'),
]

































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG548_RUNTIME_MODERN')]
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
