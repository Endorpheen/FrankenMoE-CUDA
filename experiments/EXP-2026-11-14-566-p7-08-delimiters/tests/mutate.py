#!/usr/bin/env python3
"""Mutation gate for the delimiters slice."""
from pathlib import Path
import subprocess, sys, os

S = Path('/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-14-566-p7-08-delimiters')
SRC = S / 'source' / 'runtime.goose'
ORIG = SRC.read_text()

M = [
 ("the role is always user", "    D[b]     = role;", "    D[b]     = ROLE_USER;"),
 ("the marker length is zero", "    D[b + 2] = s.len;", "    D[b + 2] = 0;"),
 ("the count does not grow", "    D[0] = n + 1;", "    D[0] = n;"),
 ("the pool cursor does not grow", "    D[1] = D[1] + s.len;", "    D[1] = D[1];"),
 ("the bound token offset is zero", "    let b = dl_dent(i);\n    D[b + 3] = D[2];", "    let b = dl_dent(i);\n    D[b + 3] = 0;"),
 ("the bound token count is zero", "    D[b + 4] = ids.len;", "    D[b + 4] = 0;"),
 ("a user start matches later positions", "        if out_role[i] == ROLE_USER && p == out_pos[i] {", "        if out_role[i] == ROLE_USER && p >= out_pos[i] {"),
 ("the last user position is the first", "    var i: i64 = n - 1;\n\n    while i >= 0 {\n        if out_role[i] == ROLE_USER {", "    var i: i64 = 0;\n\n    while i >= 0 {\n        if out_role[i] == ROLE_USER {"),
 ("user names parse as assistant", '    if dl_bytes_eq(s, "user") {\n        return ROLE_USER;\n    }', '    if dl_bytes_eq(s, "user") {\n        return ROLE_ASSISTANT;\n    }'),
 ("the span length ignores its start", "                    out_len[n] = i - prev_pos;", "                    out_len[n] = i;"),
 ("the previous position is not updated", "            prev_pos = i;", "            prev_pos = 0;"),
 ("skip regions are not skipped", "        if s < n_skips && i == sk_start[s] {", "        if false {"),
]

def run():
    env = dict(os.environ, FG571_OPT='-O0')
    try:
        r = subprocess.run(['bash', str(S / 'tests' / 'build.sh')], text=True, capture_output=True,
                           timeout=300, errors='replace', env=env)
    except subprocess.TimeoutExpired:
        return 1, 'timeout'
    line = [x for x in (r.stdout + r.stderr).splitlines() if x.startswith('FG571_DELIM cases=')]
    if line:
        return r.returncode, line[-1]
    return 1, 'no gate line'

lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
try:
    for label, old, new in M[lo:hi]:
        if ORIG.count(old) != 1:
            print('[skip]', label, 'anchor count', ORIG.count(old)); continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, line = run()
        ok = rc != 0
        caught += ok
        print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
finally:
    SRC.write_text(ORIG)
    rc, line = run()
    print(f'baseline: rc={rc} {line}')
print(f'mutations {caught}/{hi-lo} caught (range {lo}:{hi})')
