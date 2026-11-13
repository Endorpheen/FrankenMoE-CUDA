#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-13-508-p7-08-jinja-runtime-stmt'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('if always takes the body', 'if v_bool(A, F, st, test) != 0 {', 'if true {'),
    ('if always takes the alternate', 'return exec_list(al1s(A, st, id), al1c(A, st, id), cnoff, cnlen, cval, A, F, B, E, out, st);',
     'return exec_list(al0s(A, st, id), al0c(A, st, id), cnoff, cnlen, cval, A, F, B, E, out, st);'),
    ('program runs one statement', '    while i < count {', '    while i < 1 {'),
    ('set does not assign', 'if ctx_set(name, rhs, cnoff, cnlen, cval, B, A, F, st, E) < 0 {',
     'if false {'),
    ('tuple assignment is reversed', 'let item = A[pool(A, st) + vcs(A, st, rhs) + i];',
     'let item = A[pool(A, st) + vcs(A, st, rhs) + (n - 1 - i)];'),
    ('tuple size is not checked', 'if vcc(A, st, rhs) != n {', 'if false {'),
    ('non-iterable tuple set is accepted', 'if vk(A, st, rhs) != V_ARRAY && vk(A, st, rhs) != V_TUPLE {',
     'if false {'),
    ('multiline set keeps its output', '        st[7] = start;\n', ''),
    ('strings are not rendered', 'if k == V_STRING {\n        return out_bytes(out, st, vstr(B, A, st, v));',
     'if false {\n        return out_bytes(out, st, vstr(B, A, st, v));'),
    ('identifier lookup ignores shadowing', 'var i: i64 = ctxc - 1;\n\n        while i >= 0 {',
     'var i: i64 = 0;\n\n        while i < ctxc {'),
    ('the error wrapper loses the position', 'p = appi(m, p, pos);', 'p = appi(m, p, 0);'),
    ('set tuple accepts non-identifiers', 'if ak(A, st, e) != K_IDENT {', 'if false {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG508_RUNTIME_STMT')]
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
