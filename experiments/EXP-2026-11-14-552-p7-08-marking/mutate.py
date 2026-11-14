#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-552-p7-08-marking'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('input strings are not marked', '    if (A[st[27] + v] & 16) != 0 {', '    if false {'),
    ('the first part is always input', '    if st[45] > 0 && st[46] != 0 {', '    if st[45] > 0 {'),
    ('the last part is always input', '    if st[45] > 0 && st[47] != 0 {', '    if st[45] > 0 {'),
    ('bos is stripped unconditionally', 'if add_bos != 0 && (marks & 1) == 0 && bos.len > 0 && e - s >= bos.len {', 'if (marks & 1) == 0 && bos.len > 0 && e - s >= bos.len {'),
    ('eos is stripped unconditionally', 'if add_eos != 0 && (marks & 2) == 0 && eos.len > 0 && e - s >= eos.len {', 'if (marks & 2) == 0 && eos.len > 0 && e - s >= eos.len {'),
    ('the first mark bit is ignored', 'if add_bos != 0 && (marks & 1) == 0 && bos.len > 0 && e - s >= bos.len {', 'if add_bos != 0 && bos.len > 0 && e - s >= bos.len {'),
    ('the last mark bit is ignored', 'if add_eos != 0 && (marks & 2) == 0 && eos.len > 0 && e - s >= eos.len {', 'if add_eos != 0 && eos.len > 0 && e - s >= eos.len {'),
    ('bos loses one byte less', '            s = s + bos.len;', '            s = s + bos.len - 1;'),
    ('eos loses one byte less', '            e = e - eos.len;', '            e = e - eos.len + 1;'),
    ('the copy starts at the text head', '    var s: i64 = 0;\n    var e: i64 = text.len;', '    var s: i64 = 1;\n    var e: i64 = text.len;'),
    ('marking does not recurse into containers', '            chat_mark_value(A, st, A[pool(A, st) + vcs(A, st, id) + i], depth + 1);', '            i = i;'),
    ('object values are not marked', '            chat_mark_value(A, st, A[pool(A, st) + vvs(A, st, id) + i], depth + 1);', '            i = i;'),
]





































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG552_RUNTIME_MARK')]
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
