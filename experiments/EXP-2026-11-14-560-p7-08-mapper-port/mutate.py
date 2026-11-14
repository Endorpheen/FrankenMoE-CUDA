#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-560-p7-08-mapper-port'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('content is not concatenated', '        st[S_CONT] = m_san(cont, st[S_CONT], input, off, len, a_is[id], a_ic[id], inv);', '        st[S_CONT] = st[S_CONT];'),
    ('reasoning is not concatenated', '        st[S_REAS] = m_san(reas, st[S_REAS], input, off, len, a_is[id], a_ic[id], inv);', '        st[S_REAS] = st[S_REAS];'),
    ('whitespace-only reasoning is kept', '    if out[O_REASONING] > 0 && m_ws_only(reas, out[O_REASONING]) {', '    if false {'),
    ('trim keeps the leading space', '    if s.len > 0 && m_is_space(s[0]) {', '    if false {'),
    ('trim keeps the trailing spaces', '    while n > 0 && m_is_space(s[n - 1]) {', '    while false {'),
    ('tool-name does not open the arguments object', '        if m_args_len(st, out) == 0 {', '        if false {'),
    ('tool-name does not flush the argument buffer', '        if st[S_BUF_LEN] > 0 {\n            m_args_set(st, out, st[S_BUF_OFF], st[S_BUF_LEN]);\n            st[S_BUF_LEN] = 0;\n        } else', '        if false {\n            m_args_set(st, out, st[S_BUF_OFF], st[S_BUF_LEN]);\n            st[S_BUF_LEN] = 0;\n        } else'),
    ('arg-name does not close the key', '        m_tgt_append(tcb, st, out, ":");', '        m_tgt_append(tcb, st, out, "");'),
    ('arg-value skips normalization', '            m_emit_norm(tcb, st, out, text);', '            m_tgt_append(tcb, st, out, text);'),
    ('tool-close does not close braces', '        while d > 0 {', '        while d < 0 {'),
    ('string value is not escaped', '            m_emit_esc_inner(tcb, st, out, text);', '            m_tgt_append(tcb, st, out, text);'),
    ('tool-name keeps the trailing space', '    if m_tag_is(tag, tid, M_TOOL_NAME) && st[S_CUR] != 0 {\n        let t = m_trim_trail(text);', '    if m_tag_is(tag, tid, M_TOOL_NAME) && st[S_CUR] != 0 {\n        let t = text;'),
]








































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG560_RUNTIME_MAPPER')]
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
