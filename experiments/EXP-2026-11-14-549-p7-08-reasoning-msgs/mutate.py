#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-549-p7-08-reasoning-msgs'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('reasoning is never emitted', '        if reas_len.len > src && reas_len[src] > 0 {', '        if false {'),
    ('empty reasoning is emitted too', '        if reas_len.len > src && reas_len[src] > 0 {', '        if reas_len.len > src && reas_len[src] >= 0 {'),
    ('the reasoning key is renamed', 'obj_key(A, st, obj, mk_str(A, F, B, st, "reasoning_content"));', 'obj_key(A, st, obj, mk_str(A, F, B, st, "reasoning"));'),
    ('the reasoning value is the content', 'obj_val(A, st, obj, mk_str(A, F, B, st, blob[reas_off[src] .. reas_off[src] + reas_len[src]]));', 'obj_val(A, st, obj, mk_str(A, F, B, st, blob[cont_off[src] .. cont_off[src] + cont_len[src]]));'),
    ('the reasoning slice uses its offset as a length', 'reas_off[src] + reas_len[src]', 'reas_off[src] + reas_off[src]'),
    ('the reasoning slice starts one byte late', 'blob[reas_off[src] ..', 'blob[reas_off[src] + 1 ..'),
    ('the reasoning value is omitted', '        if has_reas {\n            obj_val(A, st, obj,', '        if false {\n            obj_val(A, st, obj,'),
    ('the reasoning key repeats the content key', '            obj_key(A, st, obj, mk_str(A, F, B, st, "reasoning_content"));', '            obj_key(A, st, obj, mk_str(A, F, B, st, "content"));'),
    ('the reasoning presence checks the first message', 'reas_len[src] > 0 {', 'reas_len[0] > 0 {'),
    ('the reasoning value is the role', 'blob[reas_off[src] .. reas_off[src] + reas_len[src]]', 'blob[roles_off[src] .. roles_off[src] + roles_len[src]]'),
    ('object member lookup always misses', 'let found = obj_lookup(A, B, F, st, obj, prop);', 'let found = 0 - 1;'),
    ('static member names are empty', 'prop = mk_str(A, F, B, st, astr(B, A, st, pn));', 'prop = mk_str(A, F, B, st, "");'),
]


































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG549_RUNTIME_REASONING')]
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
