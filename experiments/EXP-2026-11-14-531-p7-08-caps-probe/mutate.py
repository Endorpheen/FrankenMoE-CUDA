#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-531-p7-08-caps-probe'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('rendered values are not marked', 'A[st[27] + v] = 1;', 'A[st[27] + v] = 0;'),
    ('the flag is written into the value-count field', 'A[st[27] + v] = 1;', 'A[st[25] + v] = 1;'),
    ('the probe always reports unused', 'return A[st[27] + v];', 'return 0;'),
    ('the probe always reports used', 'return A[st[27] + v];', 'return 1;'),
    ('the probe matches the wrong key', 'if bytes_is(ks, "content") {', 'if false {'),
    ('the probe reports the key flag', 'let v = A[pool(A, st) + vvs(A, st, first) + i];', 'let v = k;'),
    ('the key offset is read as a length', 'let koff = A[st[18] + k];', 'let koff = A[st[19] + k];'),
    ('the key length is read as an offset', 'let klen = A[st[19] + k];', 'let klen = A[st[18] + k];'),
    ('new values start used', 'A[st[27] + id] = 0;', 'A[st[27] + id] = 1;'),
    ('the probe ignores the message array', 'if vcc(A, st, msgs) == 0 {', 'if true {'),
    ('the probe walk has no keys', 'let n = vkc(A, st, first);', 'let n = 0;'),
    ('the probe matches any key', 'if bytes_is(ks, "content") {', 'if klen > 0 {'),
]















def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG531_RUNTIME_CAPS')]
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
