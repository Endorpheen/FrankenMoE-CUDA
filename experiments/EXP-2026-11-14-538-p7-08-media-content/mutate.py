#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-538-p7-08-media-content'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('media detection is inverted', 'if bytes_is(ty, "media_marker") {\n            has_media = 1;', 'if bytes_is(ty, "media_marker") {\n            has_media = 0;'),
    ('media detection looks for text', 'if bytes_is(ty, "media_marker") {\n            has_media = 1;', 'if bytes_is(ty, "text") {\n            has_media = 1;'),
    ('media does not suppress the newline', 'if last_media == 0 && p > 0 {', 'if p > 0 {'),
    ('media gets a newline', 'add_nl = 0;\n                last_media = 1;', 'add_nl = 1;\n                last_media = 1;'),
    ('the media flag stays set', 'last_media = 0;', 'last_media = 1;'),
    ('unknown parts skip the next part', 'i = i + 1;\n                continue;', 'i = i + 2;\n                continue;'),
    ('empty parts produce an array', 'if n == 0 {\n        return esc(out, 0, "");', 'if false {\n        return esc(out, 0, "");'),
    ('the array type key is renamed', 'q = apps(out, q, "{\\"type\\": ");', 'q = apps(out, q, "{\\"kind\\": ");'),
    ('the array separator is a semicolon', 'q = appc(out, q, 44);', 'q = appc(out, q, 59);'),
    ('the joined string is not quoted', 'return esc(out, 0, out[base .. base + p]);', 'return app(out, 0, out, base, p);'),
    ('the array loses its last part', '    while i < n {\n        if i > 0 {', '    while i + 1 < n {\n        if i > 0 {'),
    ('the array text is not escaped', 'q = esc(out, q, blob[text_off[i] .. text_off[i] + text_len[i]]);', 'q = app(out, q, blob, text_off[i], text_len[i]);'),
]























def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG538_RUNTIME_MEDIA')]
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
