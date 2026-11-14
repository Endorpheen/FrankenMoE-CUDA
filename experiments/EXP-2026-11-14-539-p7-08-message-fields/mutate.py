#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-539-p7-08-message-fields'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('empty reasoning is emitted', '    if reas_len > 0 {', '    if true {'),
    ('the tool_call_id key is renamed', ', \\"tool_call_id\\": ', ', \\"tool_call\\": '),
    ('the call type is dropped', '{\\"type\\": \\"function\\", \\"function\\": {\\"name\\": ', '{\\"function\\": {\\"name\\": '),
    ('the call id key is renamed', ', \\"id\\": ', ', \\"call_id\\": '),
    ('empty call ids are emitted', '            if tc_id_len[i] > 0 {', '            if true {'),
    ('the arguments are not escaped', 'q = esc(out, q, blob[tc_args_off[i] .. tc_args_off[i] + tc_args_len[i]]);', 'q = app(out, q, blob, tc_args_off[i], tc_args_len[i]);'),
    ('the content key is renamed', ', \\"content\\": ', ', \\"text\\": '),
    ('the role key is renamed', '{\\"role\\": ', '{\\"author\\": '),
    ('the calls are not separated', '                q = appc(out, q, 44);', '                q = appc(out, q, 32);'),
    ('the message closes with an array', '    return appc(out, q, 125);', '    return appc(out, q, 93);'),
    ('the reasoning key is renamed', ', \\"reasoning_content\\": ', ', \\"reasoning\\": '),
    ('the tool calls array closes early', '        q = appc(out, q, 93);', '        q = appc(out, q, 125);'),
]
























def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG539_RUNTIME_FIELDS')]
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
