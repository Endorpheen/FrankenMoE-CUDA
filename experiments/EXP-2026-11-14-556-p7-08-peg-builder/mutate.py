#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-556-p7-08-peg-builder'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('string lengths are dropped', '    dcount[sid] = text.len;', '    dcount[sid] = 0;'),
    ('the pool cursor is not advanced', '    st[62] = at + text.len;', '    st[62] = at;'),
    ('the string index is not advanced', '    st[63] = sid + 1;', '    st[63] = sid;'),
    ('the node count is not advanced', '    st[60] = id + 1;', '    st[60] = id;'),
    ('a parent starts at the first child', '    cfirst[id] = st[61];', '    cfirst[id] = 0;'),
    ('a parent records no children', '    ccount[id] = n;', '    ccount[id] = 0;'),
    ('the tag does not store its child', '    let id = peg_node(kind, cfirst, ccount, sidx, st, PEG_TAG, sid);\n    children[st[61]] = child;', '    let id = peg_node(kind, cfirst, ccount, sidx, st, PEG_TAG, sid);'),
    ('the repetition bounds are swapped', '    mn[id] = lo;', '    mn[id] = hi;'),
    ('atomic is built as a tag', '    return peg_child_one(kind, cfirst, ccount, children, sidx, st, PEG_ATOMIC, 0 - 1, child);', '    return peg_child_one(kind, cfirst, ccount, children, sidx, st, PEG_TAG, 0 - 1, child);'),
    ('a sequence is built as a choice', '    return peg_flat(kind, cfirst, ccount, children, sidx, st, PEG_SEQ, kids);', '    return peg_flat(kind, cfirst, ccount, children, sidx, st, PEG_CHOICE, kids);'),
    ('nested sequences are not flattened', '        if kind[kid] == want {', '        if false {'),
    ('the flattened children come from the wrong range', '                children[st[61] + n] = children[cfirst[kid] + j];', '                children[st[61] + n] = children[j];'),
    ('the tag name and child lose their comma', '        p = peg_app(out, p, "Tag(");\n        p = peg_app(out, p, pool[dstart[sid] .. dstart[sid] + dcount[sid]]);\n        p = peg_app(out, p, ", ");', '        p = peg_app(out, p, "Tag(");\n        p = peg_app(out, p, pool[dstart[sid] .. dstart[sid] + dcount[sid]]);\n        p = peg_app(out, p, " ");'),
    ('a bounded repetition prints its minimum', '            p = peg_app_i64(out, p, mx[id]);', '            p = peg_app_i64(out, p, mn[id]);'),
]








































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG556_RUNTIME_PEG')]
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
