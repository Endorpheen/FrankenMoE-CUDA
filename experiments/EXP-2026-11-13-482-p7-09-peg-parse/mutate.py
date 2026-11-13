#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-482-p7-09-peg-parse'
SRC = E / 'source/pegparse.goose'
ORIG = SRC.read_text()

M = [
    ('a literal at EOF always fails',
     '    if lenient==0 { return R_FAIL,start,start; }\n    return R_NEED,start,pos;',
     '    return R_FAIL,start,start;'),
    ('a literal failure consumes the prefix',
     '   if input[pos]!=strs[off+i] { return R_FAIL,start,start; }',
     '   if input[pos]!=strs[off+i] { return R_FAIL,start,pos; }'),
    ('a sequence failure reports its start',
     '   if t==R_FAIL { return R_FAIL,start,e; }',
     '   if t==R_FAIL { return R_FAIL,start,start; }'),
    ('a sequence NEED_MORE reports its start',
     '   if t==R_NEED { return R_NEED,start,e; }',
     '   if t==R_NEED { return R_NEED,start,start; }'),
    ('choice treats NEED_MORE as a failure',
     '   if t!=R_FAIL { return t,s,e; }',
     '   if t==R_OK { return t,s,e; }'),
    ('repetition allows one extra match',
     '   if mxv!=-1 && count>=mxv { looping=false; } else {',
     '   if mxv!=-1 && count>mxv { looping=false; } else {'),
    ('repetition tries the child at EOF',
     '    if pos>=input.len { looping=false; } else {',
     '    if pos>input.len { looping=false; } else {'),
    ('the minimum check accepts one fewer',
     '  if mnv>0 && count<mnv {',
     '  if mnv>0 && count<=mnv {'),
    ('the lenient minimum check is inverted',
     '   if pos>=input.len && lenient!=0 { return R_NEED,start,pos; }',
     '   if pos>=input.len && lenient==0 { return R_NEED,start,pos; }'),
    ('incomplete UTF-8 is invalid even when lenient',
     '  if uk==1 {\n   if lenient==0 { return R_FAIL,start,start; }\n   return R_NEED,start,start;\n  }',
     '  if uk==1 { return R_FAIL,start,start; }'),
    ('invalid UTF-8 consumes one byte',
     '  if uk==2 { return R_FAIL,start,start; }',
     '  if uk==2 { return R_OK,start,start+1; }'),
    ('three-byte UTF-8 consumes two',
     '  return 0,3;',
     '  return 0,2;'),
    ('space only skips space and tab',
     " if c==' ' || c=='\\t' || c=='\\n' || c=='\\r' { return true; }",
     " if c==' ' || c=='\\t' { return true; }"),
    ('the two-byte continuation check is removed',
     '  if (input[off+1] & 192)!=128 { return 2,0; }',
     '  if false { return 2,0; }'),
    ('repetition swallows NEED_MORE',
     '     } else if t==R_NEED {\n      return R_NEED,start,e;',
     '     } else if t==R_NEED {\n      looping=false;'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG482_PEGPARSE')]
    if line:
        return r.returncode, line[-1]
    if 'wrote ' not in r.stdout:
        return None, 'emission failed'
    last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no stderr'
    return max(r.returncode, 1), 'build failure: ' + last[:140]


caught = 0
try:
    for label, old, new in M:
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
    run()
print(f'mutations {caught}/{len(M)} caught')
