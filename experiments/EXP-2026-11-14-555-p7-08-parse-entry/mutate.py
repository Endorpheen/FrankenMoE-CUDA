#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-555-p7-08-parse-entry'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('the generation prompt is not prepended', '    while n < gp.len {', '    while n < 0 {'),
    ('the text is not copied', '    while i < text.len {', '    while i < 0 {'),
    ('the prompt is appended after the text', '    while n < gp.len {\n        out[n] = gp[n];\n        n = n + 1;\n    }\n    var i: i64 = 0;\n\n    while i < text.len {\n        out[n] = text[i];\n        n = n + 1;\n        i = i + 1;\n    }', '    var i: i64 = 0;\n\n    while i < text.len {\n        out[n] = text[i];\n        n = n + 1;\n        i = i + 1;\n    }\n    var g: i64 = 0;\n\n    while g < gp.len {\n        out[n] = gp[g];\n        n = n + 1;\n        g = g + 1;\n    }'),
    ('the prompt is copied in reverse', '        out[n] = gp[n];', '        out[n] = gp[gp.len - 1 - n];'),
    ('the prompt loses its last byte', '    while n < gp.len {', '    while n < gp.len - 1 {'),
    ('the text is copied in reverse', '        out[n] = text[i];', '        out[n] = text[text.len - 1 - i];'),
    ('the text starts one byte late', '    var i: i64 = 0;\n\n    while i < text.len {\n        out[n] = text[i];\n        n = n + 1;\n        i = i + 1;\n    }', '    var i: i64 = 1;\n\n    while i < text.len {\n        out[n] = text[i];\n        n = n + 1;\n        i = i + 1;\n    }'),
    ('the output starts at the second byte', '    var n: i64 = 0;\n\n    while n < gp.len {\n        out[n] = gp[n];\n        n = n + 1;\n    }', '    var n: i64 = 1;\n\n    while n < gp.len {\n        out[n] = gp[n];\n        n = n + 1;\n    }'),
    ('the length omits the prompt', '        i = i + 1;\n    }\n    return n;', '        i = i + 1;\n    }\n    return n - gp.len;'),
    ('the length is the prompt only', '        i = i + 1;\n    }\n    return n;', '        i = i + 1;\n    }\n    return gp.len;'),
    ('the text overwrites the prompt', '    while i < text.len {\n        out[n] = text[i];\n        n = n + 1;\n        i = i + 1;\n    }', '    while i < text.len {\n        out[i] = text[i];\n        n = i + 1;\n        i = i + 1;\n    }\n    n = text.len;'),
    ('the length is the text only', '        i = i + 1;\n    }\n    return n;', '        i = i + 1;\n    }\n    return text.len;'),
]








































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG555_RUNTIME_PARSE')]
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
