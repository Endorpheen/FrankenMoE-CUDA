#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-541-p7-08-tools-parse'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('an empty array is malformed', '    if p < t.len && t[p] == 93 {\n        return 0;\n    }', '    if false {\n        return 0;\n    }'),
    ('objects are accepted as a tools array', 'if p >= t.len || t[p] != 91 {', 'if p >= t.len || t[p] != 123 {'),
    ('the tool type is tool', 'if te - ts - 2 != 8 || !bytes_is(t[ts + 1 .. te - 1], "function") {', 'if te - ts - 2 != 8 || !bytes_is(t[ts + 1 .. te - 1], "tool") {'),
    ('the tool type length is seven', 'if te - ts - 2 != 8 || !bytes_is(t[ts + 1 .. te - 1], "function") {', 'if te - ts - 2 != 7 || !bytes_is(t[ts + 1 .. te - 1], "function") {'),
    ('a missing type is accepted', 'if js_member(t, ostart, oend, "type", span) == 0 {', 'if false {'),
    ('a missing name is accepted', 'if js_member(t, fs, fe, "name", nspan) == 0 {', 'if false {'),
    ('the description keeps its opening quote', 'k = dspan[0] + 1;', 'k = dspan[0];'),
    ('the default parameters are an array', '            out[np] = 123;\n            np = np + 1;\n            out[np] = 125;', '            out[np] = 91;\n            np = np + 1;\n            out[np] = 93;'),
    ('the name keeps its opening quote', 'var k: i64 = nspan[0] + 1;', 'var k: i64 = nspan[0];'),
    ('tools are separated by a semicolon', 'if after < t.len && t[after] == 44 {', 'if after < t.len && t[after] == 59 {'),
    ('a closed array reports no tools', '            return count;', '            return 0;'),
    ('the member walk stops after the first key', 'if after < obj_end && t[after] == 44 {', 'if false {'),
]


























def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG541_RUNTIME_TOOLPARSE')]
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
