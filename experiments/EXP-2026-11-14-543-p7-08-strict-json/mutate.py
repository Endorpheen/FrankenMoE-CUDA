#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-543-p7-08-strict-json'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('raw control bytes are accepted', '        if c < 32 {\n            return 0 - 1;\n        }', '        if false {\n            return 0 - 1;\n        }'),
    ('the \\u escape accepts non-hex digits', 'if !is_hex_digit(t[p + 2 + k]) {', 'if false {'),
    ('a named escape advances one byte', '            } else if e == 34 || e == 92 || e == 47 || e == 98 || e == 102 || e == 110 || e == 114 || e == 116 {\n                p = p + 2;', '            } else if e == 34 || e == 92 || e == 47 || e == 98 || e == 102 || e == 110 || e == 114 || e == 116 {\n                p = p + 1;'),
    ('the tab escape leaves the set', '} else if e == 34 || e == 92 || e == 47 || e == 98 || e == 102 || e == 110 || e == 114 || e == 116 {', '} else if e == 34 || e == 92 || e == 47 || e == 98 || e == 102 || e == 110 || e == 114 {'),
    ('a leading zero is accepted', '        if t[q] == 48 {', '        if false {'),
    ('a fraction needs no digits', '        if q < t.len && t[q] == 46 {\n            q = q + 1;\n\n            if q >= t.len || !is_digit(t[q]) {\n                return 0 - 1;\n            }', '        if q < t.len && t[q] == 46 {\n            q = q + 1;\n\n            if false {\n                return 0 - 1;\n            }'),
    ('an exponent needs no digits', '        if q < t.len && (t[q] == 101 || t[q] == 69) {\n            q = q + 1;\n\n            if q < t.len && (t[q] == 43 || t[q] == 45) {\n                q = q + 1;\n            }\n\n            if q >= t.len || !is_digit(t[q]) {\n                return 0 - 1;\n            }', '        if q < t.len && (t[q] == 101 || t[q] == 69) {\n            q = q + 1;\n\n            if q < t.len && (t[q] == 43 || t[q] == 45) {\n                q = q + 1;\n            }\n\n            if false {\n                return 0 - 1;\n            }'),
    ('contains_media always reports true', '        if bytes_is(ty, "media_marker") {\n            return 1;\n        }\n        i = i + 1;\n    }\n    return 0;\n}', '        if bytes_is(ty, "media_marker") {\n            return 1;\n        }\n        i = i + 1;\n    }\n    return 1;\n}'),
    ('a lone minus is accepted', '        if t[q] == 45 {\n            q = q + 1;\n        }\n\n        if q >= t.len || !is_digit(t[q]) {\n            return 0 - 1;\n        }', '        if t[q] == 45 {\n            q = q + 1;\n        }\n\n        if q >= t.len {\n            return 0 - 1;\n        }'),
    ('media detection looks for text', '        if bytes_is(ty, "media_marker") {\n            return 1;\n        }', '        if bytes_is(ty, "text") {\n            return 1;\n        }'),
    ('the \\u escape is not recognised', '        if e == 117 {', '        if false {'),
    ('a \\u escape checks only three hex digits', '                var k: i64 = 0;\n\n                while k < 4 {\n                    if !is_hex_digit(t[p + 2 + k]) {', '                var k: i64 = 0;\n\n                while k < 3 {\n                    if !is_hex_digit(t[p + 2 + k]) {'),
]




























def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG543_RUNTIME_STRICT')]
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
