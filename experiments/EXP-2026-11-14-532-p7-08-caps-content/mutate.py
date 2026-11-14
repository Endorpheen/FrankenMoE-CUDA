#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-532-p7-08-caps-content'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('rendered values lose the used bit', 'A[st[27] + v] = A[st[27] + v] | 1;', 'A[st[27] + v] = A[st[27] + v] | 0;'),
    ('string indexing sets no bit', 'let txt = vstr(B, A, st, obj);\n\n            if idx >= 0 && idx < txt.len {\n                A[st[27] + obj] = A[st[27] + obj] | 2;', 'let txt = vstr(B, A, st, obj);\n\n            if idx >= 0 && idx < txt.len {\n                A[st[27] + obj] = A[st[27] + obj] | 0;'),
    ('rendering sets the array bit', 'A[st[27] + v] = A[st[27] + v] | 1;', 'A[st[27] + v] = A[st[27] + v] | 2;'),
    ('the content mask returns zero', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return A[st[27] + v];', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return 0;'),
    ('the content mask adds the array bit', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return A[st[27] + v];', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return A[st[27] + v] | 2;'),
    ('the content mask adds the select bit', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return A[st[27] + v];', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return A[st[27] + v] | 4;'),
    ('the content mask drops the array bit', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return A[st[27] + v];', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return A[st[27] + v] & 5;'),
    ('the content mask shifts the bits', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return A[st[27] + v];', 'if bytes_is(B[koff .. koff + klen], "content") {\n            let v = A[pool(A, st) + vvs(A, st, first) + i];\n            return A[st[27] + v] >> 1;'),
    ('the content walk matches any key', 'if bytes_is(B[koff .. koff + klen], "content") {', 'if true {'),
    ('the content walk matches any length', 'if bytes_is(B[koff .. koff + klen], "content") {', 'if klen > 0 {'),
    ('the content walk returns the key mask', 'let v = A[pool(A, st) + vvs(A, st, first) + i];', 'let v = k;'),
    ('the content walk returns the object mask', 'let v = A[pool(A, st) + vvs(A, st, first) + i];', 'let v = first;'),
]

















def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG532_RUNTIME_CAPS2')]
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
