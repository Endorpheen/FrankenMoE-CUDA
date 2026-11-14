#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-540-p7-08-tools-schema'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('empty tools become an array', '        return apps(out, 0, "null");', '        return apps(out, 0, "[]");'),
    ('the tool type is tool', 'q = apps(out, q, "{\\"type\\": \\"function\\", \\"function\\": {\\"name\\": ");', 'q = apps(out, q, "{\\"type\\": \\"tool\\", \\"function\\": {\\"name\\": ");'),
    ('the tool name key is renamed', 'q = apps(out, q, "{\\"type\\": \\"function\\", \\"function\\": {\\"name\\": ");', 'q = apps(out, q, "{\\"type\\": \\"function\\", \\"function\\": {\\"title\\": ");'),
    ('the description key is renamed', ', \\"description\\": ', ', \\"desc\\": '),
    ('the parameters key is renamed', ', \\"parameters\\": ', ', \\"params\\": '),
    ('the parameters are quoted', '        q = app(out, q, blob, parm_off[i], parm_len[i]);', '        q = esc(out, q, blob[parm_off[i] .. parm_off[i] + parm_len[i]]);'),
    ('the fallback ignores empty objects', '    if len == 0 || bytes_is(raw, "null") || bytes_is(raw, "{}") {', '    if len == 0 || bytes_is(raw, "null") {'),
    ('the fallback never triggers', '    if len == 0 || bytes_is(raw, "null") || bytes_is(raw, "{}") {', '    if false {'),
    ('the fallback type is string', '        return apps(out, 0, "{\\"type\\": \\"object\\", \\"properties\\": {}}");', '        return apps(out, 0, "{\\"type\\": \\"string\\", \\"properties\\": {}}");'),
    ('the fallback adds a required field', '        return apps(out, 0, "{\\"type\\": \\"object\\", \\"properties\\": {}}");', '        return apps(out, 0, "{\\"type\\": \\"object\\", \\"properties\\": {}, \\"required\\": []}");'),
    ('the tools are separated by a semicolon', '        if i > 0 {\n            q = appc(out, q, 44);\n        }', '        if i > 0 {\n            q = appc(out, q, 59);\n        }'),
    ('the tool object is not closed', '        q = apps(out, q, ", \\"parameters\\": ");\n        q = app(out, q, blob, parm_off[i], parm_len[i]);\n        q = appc(out, q, 125);', '        q = apps(out, q, ", \\"parameters\\": ");\n        q = app(out, q, blob, parm_off[i], parm_len[i]);'),
]

























def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG540_RUNTIME_TOOLSJSON')]
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
