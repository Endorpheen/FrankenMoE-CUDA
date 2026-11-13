#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-13-510-p7-08-jinja-macro-call'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('positional argument ignored', 'v = vchild(A, st, argv, i);', 'v = mk_int(A, F, st, 0);'),
    ('default value ignored', 'v = eval_node(ac1(A, st, pnode), st[11], noff, nlen, cval, A, F, B, E, out, st);', 'v = mk_int(A, F, st, 0);'),
    ('parameter name ignored', 'if ctx_set(pname, vals[i], noff, nlen, cval, B, A, F, st, E) < 0 {', 'if ctx_set("z", vals[i], noff, nlen, cval, B, A, F, st, E) < 0 {'),
    ('macro body not executed', 'let r = exec_list(bstart, bcount, noff, nlen, cval, A, F, B, E, out, st);', 'let r: i64 = 0;'),
    ('macro output not captured', 'let n = st[7] - start;', 'let n = 0;'),
    ('function value stores the wrong node', 'A[st[17] + f] = id;', 'A[st[17] + f] = 0;'),
    ('caller is not bound', 'if ctx_set("caller", f, cnoff, cnlen, cval, B, A, F, st, E) < 0 {', 'if false {'),
    ('caller function stores the wrong node', 'A[st[17] + f] = id;\n\n    if ctx_set("caller"', 'A[st[17] + f] = 0;\n\n    if ctx_set("caller"'),
    ('call statement result is not rendered', 'append_rendered(res, A, B, F, st, out, E);', ''),
    ('call statement does not invoke', 'let res = invoke_func(callee, argv, vcc(A, st, argv), id, K_CALLSTMT, cnoff, cnlen, cval, A, F, B, E, out, st);', 'let res: i64 = 0;'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG510_RUNTIME_MACRO')]
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
