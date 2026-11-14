#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-544-p7-08-format-single'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('the newline is never preserved', '    if add_ass != 0 && past.len > 0 && past[past.len - 1] == 10 {', '    if false {'),
    ('the newline is always emitted', '    if add_ass != 0 && past.len > 0 && past[past.len - 1] == 10 {', '    if true {'),
    ('the newline ignores add_ass', '    if add_ass != 0 && past.len > 0 && past[past.len - 1] == 10 {', '    if past.len > 0 && past[past.len - 1] == 10 {'),
    ('the tail starts one byte early', '    var i: i64 = past.len;', '    var i: i64 = past.len - 1;'),
    ('the tail starts one byte late', '    var i: i64 = past.len;', '    var i: i64 = past.len + 1;'),
    ('the tail starts at zero', '    var i: i64 = past.len;', '    var i: i64 = 0;'),
    ('the tail is empty', '    var i: i64 = past.len;', '    var i: i64 = full.len;'),
    ('the newline is a space', '        out[p] = 10;\n        p = p + 1;', '        out[p] = 32;\n        p = p + 1;'),
    ('the tail loop includes one extra byte', '    while i < full.len {', '    while i <= full.len {'),
    ('the cursor is not advanced', '        out[p] = full[i];\n        p = p + 1;', '        out[p] = full[i];'),
    ('the newline is written twice', '    if add_ass != 0 && past.len > 0 && past[past.len - 1] == 10 {\n        out[p] = 10;\n        p = p + 1;\n    }', '    if add_ass != 0 && past.len > 0 && past[past.len - 1] == 10 {\n        out[p] = 10;\n        p = p + 1;\n        out[p] = 10;\n        p = p + 1;\n    }'),
    ('the tail skips every other byte', '        out[p] = full[i];\n        p = p + 1;\n        i = i + 1;', '        out[p] = full[i];\n        p = p + 1;\n        i = i + 2;'),
]





























def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG544_RUNTIME_SINGLE')]
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
