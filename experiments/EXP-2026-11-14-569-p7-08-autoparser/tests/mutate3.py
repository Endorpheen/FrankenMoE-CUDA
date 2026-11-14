#!/usr/bin/env python3
# Mutation harness for slice 3. Six mutations in the JSON-native half and six
# in the tagged/call-id half. Each must make the 21-case gate fail.
from pathlib import Path
import subprocess
import sys

ROOT = Path('/tmp/opencode/sub-569')
SRC = ROOT / 'base/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('json', 'array wrapper ignored',
     '    if array_wrapped != 0 {',
     '    if array_wrapped == 2 {'),
    ('json', 'OpenAI type field disabled',
     '        if accept_wrapper != 0 {',
     '        if accept_wrapper == 2 {'),
    ('json', 'id field becomes mandatory',
     '), 0, 1);\n            ranks[nf] = rank_id;',
     '), 1, 1);\n            ranks[nf] = rank_id;'),
    ('json', 'parallel separator renamed',
     '        let sep = ap_seq3(kind, cfirst, ccount, children, sidx, st,\n            ap_space(kind, cfirst, ccount, sidx, st),\n            ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, ","),',
     '        let sep = ap_seq3(kind, cfirst, ccount, children, sidx, st,\n            ap_space(kind, cfirst, ccount, sidx, st),\n            ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, ";"),'),
    ('json', 'response fence renamed',
     'ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, "```json"),',
     'ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, "```"),'),
    ('json', 'plain response block loses leading space',
     '    let plain = ap_seq3(kind, cfirst, ccount, children, sidx, st,\n        ap_space(kind, cfirst, ccount, sidx, st), rf,',
     '    let plain = ap_seq3(kind, cfirst, ccount, children, sidx, st,\n        ap_eps(kind, cfirst, ccount, sidx, st), rf,'),
    ('tagged', 'call-id position check shifted',
     '    if cid_pos != 2 || cid_prefix.len == 0 || (cid_suffix.len == 0 && args_start.len == 0) {',
     '    if cid_pos != 1 || cid_prefix.len == 0 || (cid_suffix.len == 0 && args_start.len == 0) {'),
    ('tagged', 'tolerate whitespace ignored',
     '        if tolerate != 0 {',
     '        if tolerate == 2 {'),
    ('tagged', 'required and optional arguments swapped',
     '            if preq[p] != 0 {',
     '            if preq[p] == 0 {'),
    ('tagged', 'string arguments use the JSON branch',
     '            if ptype[p] == 1 {',
     '            if ptype[p] == 2 {'),
    ('tagged', 'tool rule loses its prefix',
     'let ruled = ap_rule(kind, cfirst, ccount, children, sidx, dstart, dcount, pool, st, nb2[0..cn2], func);',
     'let ruled = ap_rule(kind, cfirst, ccount, children, sidx, dstart, dcount, pool, st, fname, func);'),
    ('tagged', 'parallel call loses trailing space',
     '                    ap_rep(kind, cfirst, ccount, children, sidx, mn, mx, st, spaced, 0, 0 - 1),\n                    ap_space(kind, cfirst, ccount, sidx, st)));',
     '                    ap_rep(kind, cfirst, ccount, children, sidx, mn, mx, st, spaced, 0, 0 - 1),\n                    ap_eps(kind, cfirst, ccount, sidx, st)));'),
]


def run():
    caught = 0
    rows = []
    try:
        for i, (half, name, old, new) in enumerate(M, 1):
            n = ORIG.count(old)
            if n != 1:
                rows.append((i, half, name, f'PATTERN COUNT {n}'))
                continue
            SRC.write_text(ORIG.replace(old, new, 1))
            try:
                r = subprocess.run(['bash', str(ROOT / 'build_gate3.sh')], text=True,
                                   capture_output=True, timeout=240, errors='replace')
            except subprocess.TimeoutExpired:
                rows.append((i, half, name, 'timeout'))
                caught += 1
                continue
            ok = ('FG_DRAFT3 cases=21 values=21 failures=0' in r.stdout and r.returncode == 0)
            rows.append((i, half, name, 'NOT CAUGHT' if ok else 'caught'))
            if not ok:
                caught += 1
    finally:
        SRC.write_text(ORIG)

    for i, half, name, res in rows:
        print(f'{i:2d} {half:6s} {name:50s} {res}')
    print(f'FG_MUT3 caught={caught}/{len(M)}')
    return 0 if caught == len(M) else 1


if __name__ == '__main__':
    sys.exit(run())
