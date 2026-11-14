#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-554-p7-08-parts'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('mark_input_based_on never marks', '    if (A[st[27] + src] & 32) != 0 {', '    if false {'),
    ('mark_input_based_on tests the first-part flag', '    if (A[st[27] + src] & 32) != 0 {', '    if (A[st[27] + src] & 16) != 0 {'),
    ('mark_input_based_on always marks', '    if (A[st[27] + src] & 32) != 0 {', '    if true {'),
    ('mark_input_based_on sets only the first-part flag', '        A[st[27] + dst] = A[st[27] + dst] | 48;', '        A[st[27] + dst] = A[st[27] + dst] | 16;'),
    ('one-to-one transforms drop the all flag', '    A[st[27] + dst] = A[st[27] + dst] | (A[st[27] + src] & 48);', '    A[st[27] + dst] = A[st[27] + dst] | (A[st[27] + src] & 16);'),
    ('one-to-one transforms drop every flag', '    A[st[27] + dst] = A[st[27] + dst] | (A[st[27] + src] & 48);', '    A[st[27] + dst] = A[st[27] + dst] | 0;'),
    ('context values are marked only as first part', '        A[st[27] + id] = A[st[27] + id] | 48;', '        A[st[27] + id] = A[st[27] + id] | 16;'),
    ('concat takes the empty left first part', '        if llen != 0 {', '        if true {'),
    ('concat ignores the left all flag', '        if !ls || (A[st[27] + l] & 32) == 0 {', '        if false {'),
    ('concat ignores the right all flag', '        if !rs || (A[st[27] + r] & 32) == 0 {', '        if false {'),
    ('format literal pieces use the first-part flag', '                    if (A[st[27] + sval] & 32) != 0 {', '                    if (A[st[27] + sval] & 16) != 0 {'),
    ('format never sets the all flag', '        if all_in != 0 {', '        if false {'),
    ('format always sets the all flag', '        if all_in != 0 {', '        if true {'),
    ('format ignores argument all flags', '                        if (A[st[27] + argv[ai]] & 32) == 0 {', '                        if false {'),
]








































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG554_RUNTIME_PARTS')]
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
