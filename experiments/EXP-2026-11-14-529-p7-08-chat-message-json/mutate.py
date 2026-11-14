#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-529-p7-08-chat-message-json'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('the role key is misspelled', 'obj_key(A, st, obj, mk_str(A, F, B, st, "role"));', 'obj_key(A, st, obj, mk_str(A, F, B, st, "rol"));'),
    ('the content key is misspelled', 'obj_key(A, st, obj, mk_str(A, F, B, st, "content"));', 'obj_key(A, st, obj, mk_str(A, F, B, st, "contents"));'),
    ('the keys are emitted in reverse order', 'obj_key(A, st, obj, mk_str(A, F, B, st, "role"));\n    obj_key(A, st, obj, mk_str(A, F, B, st, "content"));', 'obj_key(A, st, obj, mk_str(A, F, B, st, "content"));\n    obj_key(A, st, obj, mk_str(A, F, B, st, "role"));'),
    ('the role value becomes the content', 'obj_val(A, st, obj, mk_str(A, F, B, st, role));', 'obj_val(A, st, obj, mk_str(A, F, B, st, content));'),
    ('the content value becomes the role', 'obj_val(A, st, obj, mk_str(A, F, B, st, content));', 'obj_val(A, st, obj, mk_str(A, F, B, st, role));'),
    ('the bare object is serialized', 'return json_into(arr, 0, -1, ", ", ": ", A, B, F, st, out, 0, E);', 'return json_into(obj, 0, -1, ", ", ": ", A, B, F, st, out, 0, E);'),
    ('the message is pushed twice', 'arr_push(A, st, arr, obj);', 'arr_push(A, st, arr, obj);\n    arr_push(A, st, arr, obj);'),
    ('the content is replaced by a literal', 'obj_val(A, st, obj, mk_str(A, F, B, st, content));', 'obj_val(A, st, obj, mk_str(A, F, B, st, "x"));'),
    ('the role is emptied', 'obj_val(A, st, obj, mk_str(A, F, B, st, role));', 'obj_val(A, st, obj, mk_str(A, F, B, st, ""));'),
    ('the content loses its last byte', 'obj_val(A, st, obj, mk_str(A, F, B, st, content));', 'obj_val(A, st, obj, mk_str(A, F, B, st, content[0..content.len - 1]));'),
    ('the array receives an empty object', 'arr_push(A, st, arr, obj);', 'arr_push(A, st, arr, vnew(A, F, st, V_OBJECT));'),
    ('both keys are role', 'obj_key(A, st, obj, mk_str(A, F, B, st, "content"));', 'obj_key(A, st, obj, mk_str(A, F, B, st, "role"));'),
]













def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG529_RUNTIME_MSGJSON')]
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
