#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-553-p7-08-flag-propagation'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('one-to-one transforms never mark', '    if (A[st[27] + src] & 16) != 0 {', '    if false {'),
    ('one-to-one transforms always mark', '    if (A[st[27] + src] & 16) != 0 {', '    if true {'),
    ('inheritance reads the destination flag', '    if (A[st[27] + src] & 16) != 0 {', '    if (A[st[27] + dst] & 16) != 0 {'),
    ('replace drops the flag', '        let res = mk_str(A, F, B, st, out[base .. base + n]);\n        inherit_input(A, st, sval, res);', '        let res = mk_str(A, F, B, st, out[base .. base + n]);'),
    ('replace inherits from its first argument', '        let res = mk_str(A, F, B, st, out[base .. base + n]);\n        inherit_input(A, st, sval, res);', '        let res = mk_str(A, F, B, st, out[base .. base + n]);\n        inherit_input(A, st, argv[0], res);'),
    ('format never marks', '        if first_flag > 0 {', '        if false {'),
    ('format always marks', '        if first_flag > 0 {', '        if true {'),
    ('format ignores argument flags', '                        if (A[st[27] + argv[ai]] & 16) != 0 {', '                        if true {'),
    ('format ignores the format-text flag', '                    if (A[st[27] + sval] & 16) != 0 {\n                        first_flag = 1;', '                    if true {\n                        first_flag = 1;'),
    ('format keeps the last flag instead of the first', '                    if first_flag < 0 {\n                        if (A[st[27] + argv[ai]] & 16) != 0 {', '                    if true {\n                        if (A[st[27] + argv[ai]] & 16) != 0 {'),
    ('join marks unconditionally', '        let res = mk_str(A, F, B, st, out[base .. base + p]);\n        st[7] = base;\n        return res;\n    }\n\n    if filter_name_is(fname, "length") {', '        let res = mk_str(A, F, B, st, out[base .. base + p]);\n        A[st[27] + res] = A[st[27] + res] | 16;\n        st[7] = base;\n        return res;\n    }\n\n    if filter_name_is(fname, "length") {'),
    ('slice drops the flag', '    inherit_input(A, st, obj, res);', ''),
    ('indent drops the flag', '        let res = mk_str(A, F, B, st, out[cstart .. p]);\n        inherit_input(A, st, sval, res);', '        let res = mk_str(A, F, B, st, out[cstart .. p]);'),
    ('split tokens lose the flag', '            let last1 = mk_str(A, F, B, st, src[cur..]);\n            inherit_input(A, st, sval, last1);', '            let last1 = mk_str(A, F, B, st, src[cur..]);'),
]







































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG553_RUNTIME_PROP')]
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
