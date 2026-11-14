#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-557-p7-08-chat-grammar'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('the until marker is dropped', '    let sid = peg_pool_add(pool, st, dstart, dcount, marker);\n    return peg_node(kind, cfirst, ccount, sidx, st, PEG_UNTIL, sid);', '    return peg_node(kind, cfirst, ccount, sidx, st, PEG_UNTIL, 0 - 1);'),
    ('the rule name is dropped', '    let sid = peg_pool_add(pool, st, dstart, dcount, name);\n    let id = peg_node(kind, cfirst, ccount, sidx, st, PEG_RULE, sid);', '    let sid = peg_pool_add(pool, st, dstart, dcount, name);\n    let id = peg_node(kind, cfirst, ccount, sidx, st, PEG_RULE, 0 - 1);'),
    ('the rule does not store its child', '    let id = peg_node(kind, cfirst, ccount, sidx, st, PEG_RULE, sid);\n    children[st[61]] = child;', '    let id = peg_node(kind, cfirst, ccount, sidx, st, PEG_RULE, sid);'),
    ('optspace drops the leading spaces', '    while a < pre_end {', '    while a < 0 {'),
    ('optspace drops the trailing spaces', '    while b < tag.len {', '    while b < suf_start {'),
    ('optspace shifts the tag body', 'tag[pre_end .. suf_start]', 'tag[pre_end .. suf_start - 1]'),
    ('prefix ignores the delimiter', '    return peg_literal_impl(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, s[0 .. cut]);', '    return peg_literal_impl(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, s);'),
    ('an empty prefix becomes a literal', '    if s.len == 0 {\n        return peg_eps_node(kind, cfirst, ccount, sidx, st);\n    }', '    if s.len == 0 {\n        return peg_literal_impl(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, s);\n    }'),
    ('the markerless safe tag consumes until the end', '        let anyv = peg_any_node(kind, cfirst, ccount, sidx, st);\n        inner = peg_tag_impl(', '        let anyv = peg_rest(kind, cfirst, ccount, sidx, st);\n        inner = peg_tag_impl('),
    ('the safe tag drops the marker guard', '        let seqb = peg_seq3(kind, cfirst, ccount, children, sidx, st, notm, anyv, untm);', '        let seqb = peg_seq3(kind, cfirst, ccount, children, sidx, st, anyv, anyv, untm);'),
    ('the safe tag repeats at least once', '    return peg_repeat(kind, cfirst, ccount, children, sidx, mn, mx, st, ch, 0, 0 - 1);', '    return peg_repeat(kind, cfirst, ccount, children, sidx, mn, mx, st, ch, 1, 0 - 1);'),
    ('the safe tag tries the rule first', '    let ch = peg_choice2(kind, cfirst, ccount, children, sidx, st, p, ruled);', '    let ch = peg_choice2(kind, cfirst, ccount, children, sidx, st, ruled, p);'),
    ('the dump never prints the until marker', '        if usid >= 0 {', '        if false {'),
    ('any dumps with parentheses', '        p = peg_app(out, p, "Any");', '        p = peg_app(out, p, "Any()");'),
]








































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG557_RUNTIME_GRAMMAR')]
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
