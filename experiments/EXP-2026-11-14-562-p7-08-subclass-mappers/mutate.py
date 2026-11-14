#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-562-p7-08-subclass-mappers'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('gemma4 looks only at direct children', '        let args_id = m_find_tag(id, tid[GT_TOOL_ARGS], 3, a_tag, a_cs, a_cc, achild);', '        let args_id = m_find_tag(id, tid[GT_TOOL_ARGS], 1, a_tag, a_cs, a_cc, achild);'),
    ('gemma4 requires a missing args node', '        if (name_id >= 0 && args_id >= 0) && a_part[name_id] == 0 {', '        if (name_id >= 0 && args_id < 0) && a_part[name_id] == 0 {'),
    ('gemma4 accepts a partial tool name', '        if (name_id >= 0 && args_id >= 0) && a_part[name_id] == 0 {', '        if (name_id >= 0 && args_id >= 0) {'),
    ('gemma4 ignores the first args child', '            if a_cc[args_id] > 0 {', '            if a_cc[args_id] > 1 {'),
    ('gemma4 emits empty nodes', '    if a_tlen[id] == 0 {\n        return at;\n    }', '    if false {\n        return at;\n    }'),
    ('gemma4 always closes a string', '            if a_part[id] == 0 {\n                p = m_appc(dst, p, \'"\');\n            }', '            if true {\n                p = m_appc(dst, p, \'"\');\n            }'),
    ('gemma4 always closes an array', "        if a_part[id] == 0 {\n            p = m_appc(dst, p, ']');\n        }", "        if true {\n            p = m_appc(dst, p, ']');\n        }"),
    ('gemma4 drops the key colon', '            p = m_app(dst, p, "\\":", 0, 2);', '            p = m_app(dst, p, "\\"", 0, 1);'),
    ('gemma4 copies the raw args text', '                let p = g_emit(\n                    achild[a_cs[args_id]], input, a_rule, a_toff, a_tlen, a_part, a_cs, a_cc, achild, rid, tcb, aoff);', '                let p = m_app(tcb, aoff, input, a_toff[args_id], a_tlen[args_id]);'),
    ('minimax looks only at direct children', '        let name_id = m_find_tag(id, tid[MT_TOOL_NAME], 3, a_tag, a_cs, a_cc, achild);\n\n        if name_id >= 0 {', '        let name_id = m_find_tag(id, tid[MT_TOOL_NAME], 1, a_tag, a_cs, a_cc, achild);\n\n        if name_id >= 0 {'),
    ('minimax always closes the container', '                MMOP_CONTAINER, id, 1, 1 - a_part[id], 0,', '                MMOP_CONTAINER, id, 1, 1, 0,'),
    ('minimax emits nameless members', '        if name_id < 0 {\n            return at, 0;\n        }', '        if false {\n            return at, 0;\n        }'),
]








































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG562_RUNTIME_SUBCLASS')]
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
