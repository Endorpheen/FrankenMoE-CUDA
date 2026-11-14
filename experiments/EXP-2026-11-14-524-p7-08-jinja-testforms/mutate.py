#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-524-p7-08-jinja-testforms'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('selectattr unknown-test text is not used', 'var r = fail_node(K_TEST, pos, out[base..p], E, st);\n\n        if mode != 0 {', 'var r = fail_node(K_TEST, pos, out[base..p], E, st);\n\n        if false {'),
    ('the two-argument form is treated as an attribute', 'if argc == 2 && vk(A, st, argv[0]) == V_STRING {', 'if false {'),
    ('the four-argument form tests the item', 'let av = attr_of(item, argv[0], undef, A, B, F, st);\n                targs[0] = argv[2];', 'let av = item;\n                targs[0] = argv[2];'),
    ('the four-argument form passes the test name', 'targs[0] = argv[2];', 'targs[0] = argv[1];'),
    ('map ignores the default keyword', 'if has_kw2 && filter_name_is(kwattr2, "default") {', 'if false {'),
    ('map uses the attribute as the default', 'miss = kwval2;', 'miss = kwval;'),
    ('a second keyword overwrites the first', 'if !has_attr {', 'if true {'),
    ('reject behaves like select', 'sel = 1 - sel;', 'sel = sel;'),
    ('the four-argument test result is ignored', 'sel = v_bool(A, F, st, r);\n            } else {', 'sel = 1;\n            } else {'),
    ('the four-argument attribute is the test name', 'let av = attr_of(item, argv[0], undef, A, B, F, st);\n                targs[0] = argv[2];', 'let av = attr_of(item, argv[1], undef, A, B, F, st);\n                targs[0] = argv[2];'),
    ('the four-argument test value is undefined', 'let av = attr_of(item, argv[0], undef, A, B, F, st);\n                targs[0] = argv[2];', 'let av = attr_of(item, argv[0], undef, A, B, F, st);\n                targs[0] = undef;'),
    ('the two-argument form reports as a test node', "dispatch_test(vstr(B, A, st, argv[0]), item, 1, targs[0..1], A, F, B, E, out, st, apos(A, st, id), 1);", "dispatch_test(vstr(B, A, st, argv[0]), item, 1, targs[0..1], A, F, B, E, out, st, apos(A, st, id), 0);"),
]








def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG524_RUNTIME_TF')]
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
