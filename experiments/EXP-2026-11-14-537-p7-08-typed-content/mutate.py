#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-537-p7-08-typed-content'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('media markers do not suppress the newline', 'if last_media == 0 && p > 0 {', 'if p > 0 {'),
    ('parts are never separated', 'if last_media == 0 && p > 0 {', 'if false {'),
    ('media markers get a newline', 'add_nl = 0;\n            last_media = 1;', 'add_nl = 1;\n            last_media = 1;'),
    ('the media flag is never cleared', 'last_media = 0;', 'last_media = 1;'),
    ('unknown parts skip the next part', 'i = i + 1;\n            continue;', 'i = i + 2;\n            continue;'),
    ('unknown parts look like media', 'i = i + 1;\n            continue;', 'last_media = 1;\n            i = i + 1;\n            continue;'),
    ('the newline rule needs two bytes', 'if last_media == 0 && p > 0 {', 'if last_media == 0 && p > 1 {'),
    ('the text slice uses the type bytes', 'let tx = blob[text_off[i] .. text_off[i] + text_len[i]];', 'let tx = blob[type_off[i] .. type_off[i] + type_len[i]];'),
    ('the newline advances two bytes', 'out[p] = 10;\n            p = p + 1;', 'out[p] = 10;\n            p = p + 2;'),
    ('the newline emission is inverted', '        if add_nl != 0 {\n            out[p] = 10;\n            p = p + 1;\n        }', '        if add_nl == 0 {\n            out[p] = 10;\n            p = p + 1;\n        }'),
    ('the last part is dropped', '    while i < n {\n        let ty = blob[type_off[i] .. type_off[i] + type_len[i]];', '    while i + 1 < n {\n        let ty = blob[type_off[i] .. type_off[i] + type_len[i]];'),
    ('every part gets a newline inside', '        if add_nl != 0 {\n            out[p] = 10;\n            p = p + 1;\n        }', '        if add_nl != 0 {\n            out[p] = 10;\n            p = p + 1;\n        }\n        out[p] = 10;\n        p = p + 1;'),
]






















def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG537_RUNTIME_TYPED')]
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
