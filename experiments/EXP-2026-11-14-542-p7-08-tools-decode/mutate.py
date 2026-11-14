#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-542-p7-08-tools-decode'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('escapes are copied verbatim', 'if c != 92 {', 'if true {'),
    ('a newline escape decodes to a space', 'out[q] = 10;\n                q = q + 1;', 'out[q] = 32;\n                q = q + 1;'),
    ('the low surrogate is ignored', 'cp = 65536 + (cp - 55296) * 1024 + (lo - 56320);', 'cp = 65536 + (cp - 55296) * 1024;'),
    ('the two-byte UTF-8 lead is wrong', 'out[p] = (192 + cp / 64) as u8;', 'out[p] = (192 + cp) as u8;'),
    ('pretty print indents one space', 'while k < lvl * 2 {', 'while k < lvl {'),
    ('pretty print drops the colon space', 'out[xs[1]] = 58;\n            xs[1] = xs[1] + 1;\n            out[xs[1]] = 32;', 'out[xs[1]] = 58;\n            xs[1] = xs[1] + 1;\n            out[xs[1]] = 0;'),
    ('compact dump adds a colon space', '            out[xs[1]] = 58;\n            xs[1] = xs[1] + 1;\n            xs[0] = js_ws(t, colon + 1);\n        } else {\n            xs[0] = js_ws(t, q);\n        }\n        js_compact(t, out, xs, depth + 1);', '            out[xs[1]] = 58;\n            out[xs[1] + 1] = 32;\n            xs[1] = xs[1] + 2;\n            xs[0] = js_ws(t, colon + 1);\n        } else {\n            xs[0] = js_ws(t, q);\n        }\n        js_compact(t, out, xs, depth + 1);'),
    ('the wrapper prefix is singular', '"Failed to parse tools: "', '"Failed to parse tool: "'),
    ('the missing-type text differs', '"Missing tool type: "', '"Missing type: "'),
    ('the suffix has no spaces', '"; tools = "', '"; tools="'),
    ('the missing-name text differs', '"[json.exception.out_of_range.403] key \'name\' not found"', '"[json.exception.out_of_range.403] key \'names\' not found"'),
    ('pretty print drops the closing newline', 'out[xs[1]] = 10;\n            xs[1] = xs[1] + 1;\n            xs[1] = pp_ind(out, xs[1], lvl);', 'out[xs[1]] = 32;\n            xs[1] = xs[1] + 1;\n            xs[1] = pp_ind(out, xs[1], lvl);'),
]



























def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG542_RUNTIME_TOOLMSG')]
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
