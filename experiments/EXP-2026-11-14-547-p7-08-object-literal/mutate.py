#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-547-p7-08-object-literal'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('the literal entry cap is one', '        if kn > 16 {', '        if kn > 1 {'),
    ('the key list is read as the value list', '            let knode = A[apool(A, st) + al0s(A, st, id) + i];', '            let knode = A[apool(A, st) + al1s(A, st, id) + i];'),
    ('the key slot stores the value', '            kk[i] = key;', '            kk[i] = val;'),
    ('the key phase stores values', '            obj_key(A, st, obj, kk[i]);', '            obj_key(A, st, obj, vv[i]);'),
    ('the value phase stores keys', '            obj_val(A, st, obj, vv[i]);', '            obj_val(A, st, obj, kk[i]);'),
    ('the rebuild copies one pair less', '            while j < m {\n                kk2[j] = A[pool(A, st) + vks(A, st, objv) + j];', '            while j < m - 1 {\n                kk2[j] = A[pool(A, st) + vks(A, st, objv) + j];'),
    ('the appended key slot holds the value', '            kk2[m] = key;', '            kk2[m] = rhs;'),
    ('the rebuild drops the new key', '            while j <= m {\n                obj_key(A, st, obj2, kk2[j]);', '            while j < m {\n                obj_key(A, st, obj2, kk2[j]);'),
    ('the binding keeps the old object', '            if ctx_set(nm, obj2, cnoff, cnlen, cval, B, A, F, st, E) < 0 {', '            if ctx_set(nm, objv, cnoff, cnlen, cval, B, A, F, st, E) < 0 {'),
    ('the rebuild forgets the existing pairs', '            let m = vkc(A, st, objv);', '            let m: i64 = 0;'),
    ('a new key falls back to the in-place append', '        if ak(A, st, ac0(A, st, assignee)) == K_IDENT {', '        if false {'),
    ('the rebuild drops the new value', '            while j <= m {\n                obj_val(A, st, obj2, vv2[j]);', '            while j < m {\n                obj_val(A, st, obj2, vv2[j]);'),
]
































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG547_RUNTIME_OBJLIT')]
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
