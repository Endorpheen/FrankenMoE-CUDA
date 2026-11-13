#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-516-p7-08-jinja-compare'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('lt compares as greater', '            op = 3;', '            op = 2;'),
    ('gt compares as less', '            op = 2;', '            op = 3;'),
    ('ge compares as greater', '            op = 1;', '            op = 2;'),
    ('lessthan is not mapped to lt', 'if filter_name_is(tname, "lt") || filter_name_is(tname, "lessthan") {', 'if filter_name_is(tname, "lt") {'),
    ('greaterthan is not mapped to gt', '} else if filter_name_is(tname, "gt") || filter_name_is(tname, "greaterthan") {', '} else if filter_name_is(tname, "gt") {'),
    ('bool ordering error is swallowed', 'if r < 0 {\n            return fail_node(K_TEST, apos(A, st, id), E[0..st[2]], E, st);', 'if false {\n            return fail_node(K_TEST, apos(A, st, id), E[0..st[2]], E, st);'),
    ('startswith compares the suffix', 'if filter_name_is(fname, "startswith") {\n                    if src[i] != pat[i] {', 'if false {\n                    if src[i] != pat[i] {'),
    ('endswith compares the prefix', '} else {\n                    if src[src.len - pat.len + i] != pat[i] {', '} else {\n                    if src[i] != pat[i] {'),
    ('prefix compare is inverted', 'if filter_name_is(fname, "startswith") {\n                    if src[i] != pat[i] {', 'if filter_name_is(fname, "startswith") {\n                    if src[i] == pat[i] {'),
    ('suffix index is shifted', 'src[src.len - pat.len + i]', 'src[src.len - pat.len - 1 + i]'),
    ('empty pattern is rejected', 'if pat.len <= src.len {', 'if pat.len < src.len {'),
    ('a match reports false', '        if hit {\n            return mk_bool(A, F, st, 1);', '        if hit {\n            return mk_bool(A, F, st, 0);'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG516_RUNTIME_CMP')]
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
