#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-550-p7-08-2026-templates'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('developer is not mapped to system', 'if bytes_is(role, "developer") {', 'if false {'),
    ('a reverse slice starts at zero', '    if !has_start && step < 0 {\n        start = len - 1;\n    }', '    if !has_start && step < 0 {\n        start = 0;\n    }'),
    ('the slice walk includes the stop', '    while dir * i < dir * tv {\n            if i >= 0 && i < len {\n                arr_push(A, st, res, A[pool(A, st) + vcs(A, st, obj) + i]);', '    while dir * i <= dir * tv {\n            if i >= 0 && i < len {\n                arr_push(A, st, res, A[pool(A, st) + vcs(A, st, obj) + i]);'),
    ('loop.index starts at zero', 'lv[0] = mk_int(A, F, st, j + 1);', 'lv[0] = mk_int(A, F, st, j);'),
    ('the loop iterates one item', 'count = vcc(A, st, iter);', 'count = 1;'),
    ('the if body is inverted', 'if v_bool(A, F, st, test) != 0 {', 'if v_bool(A, F, st, test) == 0 {'),
    ('object member lookup always misses', 'let found = obj_lookup(A, B, F, st, obj, prop);', 'let found = 0 - 1;'),
    ('the ternary condition is inverted', '        if v_bool(A, F, st, cond) != 0 {', '        if v_bool(A, F, st, cond) == 0 {'),
    ('comments are rendered as values', '    if k == K_COMMENT {\n        return 0;\n    }', '    if false {\n        return 0;\n    }'),
    ('string slices read from the string head', '            out[base + n] = B[vfo(A, st, obj) + i];', '            out[base + n] = B[i];'),
    ('member assignment stores the key', '                A[st[26] + vvs(A, st, objv) + i] = rhs;', '                A[st[26] + vvs(A, st, objv) + i] = k;'),
    ('the reasoning key repeats the content key', 'obj_key(A, st, obj, mk_str(A, F, B, st, "reasoning_content"));', 'obj_key(A, st, obj, mk_str(A, F, B, st, "content"));'),
]



































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG550_RUNTIME_2026')]
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
