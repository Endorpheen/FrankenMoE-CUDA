#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-561-p7-08-visited-depth'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('the search ignores the area base', '    var vi: i64 = vbase;', '    var vi: i64 = 0;'),
    ('the search window is empty', '    while vi < st[71] {', '    while vi < vbase {'),
    ('a seen node is not appended', '    vis[st[71]] = id;', '    vis[0] = id;'),
    ('the cursor does not advance', '    st[71] = st[71] + 1;', '    st[71] = st[71];'),
    ('the tag area starts at zero', '        let vbase2 = st[71];', '        let vbase2 = 0;'),
    ('the tag does not restore the cursor', '        st[71] = vbase2;', '        st[71] = st[71];'),
    ('the tag area starts one below', '        let vbase2 = st[71];', '        let vbase2 = st[71] - 1;'),
    ('the search window uses the arena size', '    while vi < st[71] {', '    while vi < st[60] {'),
    ('the lookup reads the other array', '        if vis[vi] == id {', '        if vis2[vi] == id {'),
    ('the tag keeps the parent area', '        let vbase2 = st[71];', '        let vbase2 = vbase;'),
    ('the marker is spelled differently', '            let cyc = peg_app(out, st[70], "[cycle]");', '            let cyc = peg_app(out, st[70], "[loop]");'),
    ('the root uses a negative base', 'let n = peg_dump_node(root, vis, vis2, 0, kind,', 'let n = peg_dump_node(root, vis, vis2, 0 - 1, kind,'),
]








































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG561_RUNTIME_VISITED')]
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
