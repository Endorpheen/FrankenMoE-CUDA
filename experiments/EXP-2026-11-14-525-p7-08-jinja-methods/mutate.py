#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-525-p7-08-jinja-methods'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('method values are plain functions', 'let f = vnew(A, F, st, V_METHOD);', 'let f = vnew(A, F, st, V_FUNC);'),
    ('objects gain a length method', 'if kind == V_OBJECT {\n        return filter_name_is(n, "get")', 'if kind == V_OBJECT {\n        return filter_name_is(n, "length") || filter_name_is(n, "get")'),
    ('the method name comes from the AST fields', 'A[st[18] + f] = A[st[18] + prop];\n    A[st[19] + f] = A[st[19] + prop];', 'A[st[18] + f] = aso(A, st, prop);\n    A[st[19] + f] = asl(A, st, prop);'),
    ('method calls lose their arguments', 'A[st[38] + cnode] = al0s(A, st, id);', 'A[st[38] + cnode] = 0;'),
    ('method calls report no arguments', 'A[st[39] + cnode] = al0c(A, st, id);', 'A[st[39] + cnode] = 0;'),
    ('the filter operand is the name node', 'A[st[35] + fnode] = vnode;', 'A[st[35] + fnode] = inode;'),
    ('object keys win over builtins', 'if aiv(A, st, id) == 0 {\n        let mv = method_value(A, F, B, st, obj, prop);', 'if aiv(A, st, id) == 0 && vk(A, st, obj) != V_OBJECT {\n        let mv = method_value(A, F, B, st, obj, prop);'),
    ('every type uses the object method table', 'if kind == V_OBJECT {', 'if true {'),
    ('method values are not callable', 'if k == V_FUNC || k == V_METHOD {\n            res = 1;', 'if k == V_FUNC {\n            res = 1;'),
    ('the receiver is lost', 'let recv = vi(A, st, callee);', 'let recv = 0;'),
    ('the operand node is an integer literal', 'A[st[30] + vnode] = K_VALUE;', 'A[st[30] + vnode] = K_INT;'),
    ('the name node is a string literal', 'A[st[30] + inode] = K_IDENT;', 'A[st[30] + inode] = K_STRING;'),
]









def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG525_RUNTIME_METH')]
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
