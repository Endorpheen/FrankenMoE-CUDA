#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-530-p7-08-chat-roles'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('developer is not mapped', 'if bytes_is(role, "developer") {\n            role = "system";\n        }', 'if false {\n            role = "system";\n        }'),
    ('the merge joins with a space', 'out[p] = 10;\n            p = p + 1;\n            k = 0;', 'out[p] = 32;\n            p = p + 1;\n            k = 0;'),
    ('the merged content is dropped', 'content = out[0..p];', 'content = out[0..0];'),
    ('a lone system message is kept', 'effective = 0;', 'effective = 1;'),
    ('the merge flag is not set', 'merged = true;', 'merged = false;'),
    ('the start index is not advanced', 'start = 1;', 'start = 0;'),
    ('caps are ignored', 'if supports_system == 0 && n > 0 {', 'if n > 0 {'),
    ('the developer check looks for assistant', 'if bytes_is(role, "developer") {', 'if bytes_is(role, "assistant") {'),
    ('the mapped role is assistant', 'if bytes_is(role, "developer") {\n            role = "system";\n        }', 'if bytes_is(role, "developer") {\n            role = "assistant";\n        }'),
    ('the effective count loses two', 'effective = n - 1;', 'effective = n - 2;'),
    ('only one array slot is reserved', 'while ri < effective {', 'while ri < 1 {'),
    ('the object keys are swapped', 'obj_key(A, st, obj, mk_str(A, F, B, st, "role"));\n        obj_key(A, st, obj, mk_str(A, F, B, st, "content"));', 'obj_key(A, st, obj, mk_str(A, F, B, st, "content"));\n        obj_key(A, st, obj, mk_str(A, F, B, st, "role"));'),
]














def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG530_RUNTIME_ROLES')]
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
