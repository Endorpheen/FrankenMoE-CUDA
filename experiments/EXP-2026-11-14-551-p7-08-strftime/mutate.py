#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-551-p7-08-strftime'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('the epoch is ignored', '            epoch = vi(A, st, epoch_v);', '            epoch = 0;'),
    ('the clock variable is misspelled', 'let epoch_v = ctx_find("now_epoch", noff, nlen, cval, B, st);', 'let epoch_v = ctx_find("now", noff, nlen, cval, B, st);'),
    ('the format is ignored', 'let n = libc_strftime_epoch(out[base ..], epoch, vstr(B, A, st, argv[0]));', 'let n = libc_strftime_epoch(out[base ..], epoch, "literal");'),
    ('the format loses its first byte', 'let n = libc_strftime_epoch(out[base ..], epoch, vstr(B, A, st, argv[0]));', 'let n = libc_strftime_epoch(out[base ..], epoch, vstr(B, A, st, argv[0])[1..]);'),
    ('the result length loses one byte', '        let res = mk_str(A, F, B, st, out[base .. base + n]);', '        let res = mk_str(A, F, B, st, out[base .. base + n - 1]);'),
    ('the result keeps one extra byte', '        let res = mk_str(A, F, B, st, out[base .. base + n]);', '        let res = mk_str(A, F, B, st, out[base .. base + n + 1]);'),
    ('strftime_now resolves to dict', '            return mk_builtin(A, F, st, 5);', '            return mk_builtin(A, F, st, 3);'),
    ('strftime_now is not resolved', '        if filter_name_is(name, "strftime_now") {', '        if false {'),
    ('the result starts one byte late', 'let n = libc_strftime_epoch(out[base ..], epoch, vstr(B, A, st, argv[0]));', 'let n = libc_strftime_epoch(out[base + 1 ..], epoch, vstr(B, A, st, argv[0]));'),
    ('the scratch cursor is not rewound', '        st[7] = base;\n        return res;\n    }\n\n    if bid == 3 || bid == 4 {', '        st[7] = base + n;\n        return res;\n    }\n\n    if bid == 3 || bid == 4 {'),
    ('the epoch is halved', '            epoch = vi(A, st, epoch_v);', '            epoch = vi(A, st, epoch_v) / 2;'),
    ('the formatted text becomes an integer', '        let res = mk_str(A, F, B, st, out[base .. base + n]);', '        let res = mk_int(A, F, st, n);'),
]




































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG551_RUNTIME_TIME')]
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
