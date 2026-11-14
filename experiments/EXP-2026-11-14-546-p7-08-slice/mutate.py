#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-546-p7-08-slice'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('a reverse slice starts at zero', '    if !has_start && step < 0 {\n        start = len - 1;\n    }', '    if !has_start && step < 0 {\n        start = 0;\n    }'),
    ('a reverse slice stops at the length', '    if !has_stop && step < 0 {\n        stop = 0 - 1;\n    }', '    if !has_stop && step < 0 {\n        stop = len;\n    }'),
    ('a negative start is negated instead', '    if step < 0 {\n        dir = 0 - 1;\n\n        if sv < 0 {\n            sv = len + sv;', '    if step < 0 {\n        dir = 0 - 1;\n\n        if sv < 0 {\n            sv = 0 - sv;'),
    ('the forward start clamps one early', '        } else if sv > len {\n            sv = len;\n        }', '        } else if sv > len - 1 {\n            sv = len - 1;\n        }'),
    ('the walk includes the stop', '    while dir * i < dir * tv {\n            if i >= 0 && i < len {\n                arr_push(A, st, res, A[pool(A, st) + vcs(A, st, obj) + i]);', '    while dir * i <= dir * tv {\n            if i >= 0 && i < len {\n                arr_push(A, st, res, A[pool(A, st) + vcs(A, st, obj) + i]);'),
    ('a zero step is accepted', '    if step == 0 {\n        return fail_node(K_MEMBER, pos, "slice step cannot be zero", E, st);\n    }', '    if false {\n        return fail_node(K_MEMBER, pos, "slice step cannot be zero", E, st);\n    }'),
    ('string slices read from the string head', '            out[base + n] = B[vfo(A, st, obj) + i];', '            out[base + n] = B[i];'),
    ('the forward stop default drops one', '    var stop: i64 = len;', '    var stop: i64 = len - 1;'),
    ('the ternary condition is inverted', '        if v_bool(A, F, st, cond) != 0 {\n            return eval_node(ac1(A, st, id), ctxc, noff, nlen, cval, A, F, B, E, out, st);\n        }', '        if v_bool(A, F, st, cond) == 0 {\n            return eval_node(ac1(A, st, id), ctxc, noff, nlen, cval, A, F, B, E, out, st);\n        }'),
    ('the ternary returns the true branch twice', '        return eval_node(ac2(A, st, id), ctxc, noff, nlen, cval, A, F, B, E, out, st);', '        return eval_node(ac1(A, st, id), ctxc, noff, nlen, cval, A, F, B, E, out, st);'),
    ('comments are rendered as values', '    if k == K_COMMENT {\n        return 0;\n    }', '    if false {\n        return 0;\n    }'),
    ('the start default is one', '    var start: i64 = 0;', '    var start: i64 = 1;'),
]































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG546_RUNTIME_SLICE')]
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
