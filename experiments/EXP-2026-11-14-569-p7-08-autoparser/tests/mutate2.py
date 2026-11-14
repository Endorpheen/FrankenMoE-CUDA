#!/usr/bin/env python3
# Mutation harness for slice 2. Each mutation edits base/runtime.goose, rebuilds
# the gate and must make it fail. Two halves: reasoning and TAG_WITH_JSON.
from pathlib import Path
import subprocess
import sys

ROOT = Path('/tmp/opencode/sub-569')
SRC = ROOT / 'base/runtime.goose'
ORIG = SRC.read_text()

M = [
    # half 1: reasoning parser and root
    ('reasoning', 'until uses start instead of trimmed end',
     'let until = ap_until(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, te);',
     'let until = ap_until(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, start);'),
    ('reasoning', 'leading optspace uses end',
     'let pre = ap_optspace(kind, cfirst, ccount, children, sidx, dstart, dcount, mn, mx, pool, st, start);',
     'let pre = ap_optspace(kind, cfirst, ccount, children, sidx, dstart, dcount, mn, mx, pool, st, te);'),
    ('reasoning', 'reasoning block becomes mandatory',
     'return ap_rep(kind, cfirst, ccount, children, sidx, mn, mx, st, inner, 0, 1);\n    }\n    let post = ap_optspace',
     'return ap_rep(kind, cfirst, ccount, children, sidx, mn, mx, st, inner, 1, 1);\n    }\n    let post = ap_optspace'),
    ('reasoning', 'plain content drops the reasoning parser',
     '        k3[0] = rp;\n        k3[1] = ap_tag(kind, cfirst, ccount, children, sidx, dstart, dcount, pool, st, "content",',
     '        k3[0] = ap_eps(kind, cfirst, ccount, sidx, st);\n        k3[1] = ap_tag(kind, cfirst, ccount, children, sidx, dstart, dcount, pool, st, "content",'),
    ('reasoning', 'wrapped content drops its start literal',
     '        k5[1] = ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, cstart);',
     '        k5[1] = ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, "");'),
    ('reasoning', 'reasoning root loses its space node',
     '    k3[0] = pre;\n    k3[1] = ap_space(kind, cfirst, ccount, sidx, st);\n    k3[2] = body;',
     '    k3[0] = pre;\n    k3[1] = ap_eps(kind, cfirst, ccount, sidx, st);\n    k3[2] = body;'),
    # half 2: TAG_WITH_JSON
    ('tools', 'tool name is not atomic',
     '            ap_atomic(kind, cfirst, ccount, children, sidx, st,\n                ap_tag(kind, cfirst, ccount, children, sidx, dstart, dcount, pool, st, "tool-name",\n                    ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, fname))),',
     '            ap_tag(kind, cfirst, ccount, children, sidx, dstart, dcount, pool, st, "tool-name",\n                ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, fname)),'),
    ('tools', 'rule names keep invalid characters',
     '            if dirty == 0 {\n                rb[n] = 45;\n                n = n + 1;\n                dirty = 1;\n            }',
     '            if dirty == 0 {\n                rb[n] = c;\n                n = n + 1;\n                dirty = 1;\n            }'),
    ('tools', 'parallel flag ignored',
     '        if parallel != 0 {\n            let rep = ap_rep',
     '        if parallel == 2 {\n            let rep = ap_rep'),
    ('tools', 'required flag inverted',
     '    if required == 0 {\n        tc = ap_rep',
     '    if required != 0 {\n        tc = ap_rep'),
    ('tools', 'tool close tag renamed',
     '"tool-close",\n                        ap_space(kind, cfirst, ccount, sidx, st))));',
     '"tool-end",\n                        ap_space(kind, cfirst, ccount, sidx, st))));'),
    ('tools', 'json object reuses the closing brace',
     '    let rbrace1 = ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, "}");\n    let rbrace2 = ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, "}");',
     '    let rbrace1 = ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, "}");\n    let rbrace2 = rbrace1;'),
]


def run():
    caught = 0
    rows = []
    try:
        for i, (half, name, old, new) in enumerate(M, 1):
            if old not in ORIG:
                rows.append((i, half, name, 'MISSING PATTERN'))
                continue
            SRC.write_text(ORIG.replace(old, new, 1))
            try:
                r = subprocess.run(['bash', str(ROOT / 'build_gate2.sh')], text=True,
                                   capture_output=True, timeout=240, errors='replace')
            except subprocess.TimeoutExpired:
                rows.append((i, half, name, 'timeout'))
                caught += 1
                continue
            ok = ('FG_DRAFT2 cases=10 values=10 failures=0' in r.stdout and r.returncode == 0)
            rows.append((i, half, name, 'NOT CAUGHT' if ok else 'caught'))
            if not ok:
                caught += 1
    finally:
        SRC.write_text(ORIG)

    for i, half, name, res in rows:
        print(f'{i:2d} {half:9s} {name:45s} {res}')
    print(f'FG_MUT2 caught={caught}/{len(M)}')
    return 0 if caught == len(M) else 1


if __name__ == '__main__':
    sys.exit(run())
