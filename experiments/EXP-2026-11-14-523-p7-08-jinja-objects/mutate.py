#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-523-p7-08-jinja-objects'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('kwargs take the key node as the value', 'let val = eval_node(ac1(A, st, argn), st[11], noff, nlen, cval, A, F, B, E, out, st);', 'let val = eval_node(ac0(A, st, argn), st[11], noff, nlen, cval, A, F, B, E, out, st);'),
    ('object copy swaps keys and values', 'kk[n + j] = A[pool(A, st) + vks(A, st, it) + j];\n                        vv[n + j] = A[pool(A, st) + vvs(A, st, it) + j];', 'kk[n + j] = A[pool(A, st) + vvs(A, st, it) + j];\n                        vv[n + j] = A[pool(A, st) + vks(A, st, it) + j];'),
    ('pair values come from the key slot', 'vv[n + j] = A[pool(A, st) + vcs(A, st, pairid) + 1];', 'vv[n + j] = A[pool(A, st) + vcs(A, st, pairid)];'),
    ('three-element pairs are accepted', '&& vcc(A, st, pairid) == 2', '&& vcc(A, st, pairid) > 0'),
    ('object copies are ignored', 'let ik = vk(A, st, it);\n\n                if ik == V_OBJECT {', 'let ik = vk(A, st, it);\n\n                if false {'),
    ('a second positional argument is accepted', 'if iter_seen != 0 {', 'if false {'),
    ('the entry cap is one', 'if n >= 8 {', 'if n >= 1 {'),
    ('the key region holds values', 'A[st[26] + kb + q] = kk[q];', 'A[st[26] + kb + q] = vv[q];'),
    ('the value region holds keys', 'A[st[26] + vb + q] = vv[q];', 'A[st[26] + vb + q] = kk[q];'),
    ('namespace is not dispatched', 'if bid == 3 || bid == 4 {', 'if bid == 3 {'),
    ('dict is not resolved as a value', 'if filter_name_is(name, "dict") {', 'if false {'),
    ('pair keys come from the value slot', 'kk[n + j] = A[pool(A, st) + vcs(A, st, pairid)];', 'kk[n + j] = A[pool(A, st) + vcs(A, st, pairid) + 1];'),
]







def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG523_RUNTIME_OBJ')]
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
