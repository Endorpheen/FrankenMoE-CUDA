#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-483-p7-09-peg-parse2'
SRC = E / 'source/pegparse2.goose'
ORIG = SRC.read_text()

M = [
    ('And consumes the child end',
     '  return t,start,start;\n }\n if k==K_NOT {',
     '  return t,start,e;\n }\n if k==K_NOT {'),
    ('Not swallows NEED_MORE',
     '  if t==R_NEED { return R_NEED,start,start; }',
     '  if t==R_NEED { return R_OK,start,start; }'),
    ('Not succeeds when the child succeeds',
     '  if t==R_OK { return R_FAIL,start,start; }',
     '  if t==R_OK { return R_OK,start,start; }'),
    ('Atomic returns success without the child',
     ' if k==K_ATOMIC || k==K_GBNF || k==K_AC {\n  return parse_node(children[cfirst[id]],start,lenient,input,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,rstart,rcount,rngs,rnge,neg,dstart,dcount,delims);',
     ' if k==K_ATOMIC || k==K_GBNF || k==K_AC {\n  return R_OK,start,start;'),
    ('Chars ignores negation',
     '     if neg[id]!=0 { m=!m; }',
     '     if neg[id]==99 { m=!m; }'),
    ('Chars allows one extra match',
     '   if mx[id]!=-1 && count>=mx[id] { stop=true; } else {',
     '   if mx[id]!=-1 && count>mx[id] { stop=true; } else {'),
    ('Chars fails on incomplete UTF-8 with enough matches',
     '     if count>=mn[id] { stop=true; } else if lenient!=0 { return R_NEED,start,pos; } else { return R_FAIL,start,start; }',
     '     if false { stop=true; } else if lenient!=0 { return R_NEED,start,pos; } else { return R_FAIL,start,start; }'),
    ('Chars fails on invalid UTF-8 with enough matches',
     '     if count>=mn[id] { stop=true; } else { return R_FAIL,start,start; }',
     '     return R_FAIL,start,start;'),
    ('Chars lenient minimum is inverted',
     '  if count<mn[id] {\n   if pos>=input.len && lenient!=0 { return R_NEED,start,pos; }',
     '  if count<mn[id] {\n   if pos>=input.len && lenient==0 { return R_NEED,start,pos; }'),
    ('String consumes its delimiter',
     '   if c==delim { return R_OK,start,pos; }',
     '   if c==delim { return R_OK,start,pos+1; }'),
    ('String accepts an unknown escape',
     ' if c==delim || c==\'\\\\\' || c==\'/\' || c==\'b\' || c==\'f\' || c==\'n\' || c==\'r\' || c==\'t\' {',
     ' if true {'),
    ('String unicode escape accepts three hex digits',
     '  var i:i64=0;\n  while i<4 {',
     '  var i:i64=0;\n  while i<3 {'),
    ('String at EOF succeeds in strict mode',
     '  if lenient==0 { return R_FAIL,start,pos; }\n  return R_NEED,start,pos;\n }\n if k==K_UNTIL {',
     '  return R_NEED,start,pos;\n }\n if k==K_UNTIL {'),
    ('Until ignores partial delimiter prefixes',
     '     if ok { hit=true; }',
     '     if ok && z==d_len { hit=true; }'),
    ('Until reports the wrong end',
     '  if last==input.len && lenient!=0 { return R_NEED,start,last; }\n  return R_OK,start,last;',
     '  if last==input.len && lenient!=0 { return R_NEED,start,last; }\n  return R_OK,start,input.len;'),
    ('Until lenient end is inverted',
     '  if last==input.len && lenient!=0 { return R_NEED,start,last; }',
     '  if last==input.len && lenient==0 { return R_NEED,start,last; }'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG483_PEGPARSE2')]
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
