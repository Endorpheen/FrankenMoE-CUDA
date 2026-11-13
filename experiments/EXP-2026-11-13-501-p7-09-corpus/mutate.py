#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-501-p7-09-corpus'

M = [
    ('gbnf: the scratch shrinks to 1024 elements', 'source/gbnf.goose',
     ' let sc:i64=rtype.len-prev_len;', ' let sc:i64=rtype.len-1024;'),
    ('gbnf: the repetition guard is looser', 'source/gbnf.goose',
     ' if n_prev_rules*total>MAX_REP {', ' if n_prev_rules*total>MAX_REP+1000 {'),
    ('peg: repetition ignores its upper bound', 'source/pegparse.goose',
     '   if mxv!=-1 && count>=mxv { looping=false; } else {', '   if mxv!=-1 && count>mxv+100000 { looping=false; } else {'),
    ('peg: a literal only compares one byte', 'source/pegparse.goose',
     '   if input[pos]!=strs[off+i] { return R_FAIL,start,start; }', '   if i>0 && input[pos]!=strs[off+i] { return R_FAIL,start,start; }'),
    ('peg2: until stops at the first byte', 'source/pegparse2.goose',
     '     while z<d_len && pos+z<input.len {', '     while z<1 && pos+z<input.len {'),
    ('peg2: chars stops after one codepoint', 'source/pegparse2.goose',
     '    if m { pos=pos+ub; count=count+1; } else { stop=true; }', '    if m { pos=pos+ub; count=count+1; stop=true; } else { stop=true; }'),
    ('peg2: a string ignores its delimiter', 'source/pegparse2.goose',
     "   if c==delim { return R_OK,start,pos; }", "   if c==(1 as u8) { return R_OK,start,pos; }"),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixtures.sh')], text=True,
                           capture_output=True, timeout=600, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    lines = [x for x in r.stdout.splitlines() if x.startswith('FG501_')]
    if len(lines) == 3:
        return r.returncode, ' | '.join(lines)
    if r.returncode == 0:
        return None, 'the fixtures did not run'
    last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no stderr'
    return max(r.returncode, 1), 'partial/failed run: ' + last[:120]


orig = {f: (E / f).read_text() for f in {m[1] for m in M}}
caught = 0
try:
    for label, rel, old, new in M:
        src = E / rel
        if old not in orig[rel]:
            print('[skip]', label)
            continue
        src.write_text(orig[rel].replace(old, new, 1))
        rc, line = run()
        if rc is None:
            print('[build-fail]', label)
            continue
        ok = rc != 0
        caught += ok
        print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
finally:
    for f, text in orig.items():
        (E / f).write_text(text)
    run()
print(f'mutations {caught}/{len(M)} caught')
