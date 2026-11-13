#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-484-p7-09-peg-ast'
SRC = E / 'source/pegast.goose'
ORIG = SRC.read_text()

M = [
    ('a node starts at zero',
     '  a_start[nid]=s;',
     '  a_start[nid]=0;'),
    ('a rule node uses the tag name slot',
     '  if k==K_RULE { a_rule[nid]=si; a_tag[nid]=-1; } else { a_rule[nid]=-1; a_tag[nid]=si; }',
     '  if k==K_RULE { a_rule[nid]=-1; a_tag[nid]=si; } else { a_rule[nid]=-1; a_tag[nid]=si; }'),
    ('the partial flag is never set',
     '  if t==R_NEED { a_partial[nid]=1; } else { a_partial[nid]=0; }',
     '  a_partial[nid]=0;'),
    ('the node text is empty',
     '  if s<input.len { a_toff[nid]=s; a_tlen[nid]=e-s; } else { a_toff[nid]=0; a_tlen[nid]=0; }',
     '  a_toff[nid]=0; a_tlen[nid]=0;'),
    ('child nodes are not recorded',
     '  while z<rc {\n   achild[cb+z]=nres[rb+z];\n   z=z+1;\n  }',
     '  while z<0 {\n   achild[cb+z]=nres[rb+z];\n   z=z+1;\n  }'),
    ('the result returns the child list instead of the node',
     '  nres[st[1]]=nid;\n  st[1]=st[1]+1;\n  return t,s,e,st[1]-1,1;',
     '  nres[st[1]]=nid;\n  st[1]=st[1]+1;\n  return t,s,e,rb,rc;'),
    ('sequences drop earlier child nodes',
     '   pos=e;\n   i=i+1;\n  }\n  return R_OK,start,pos,base,st[1]-base;',
     '   pos=e;\n   st[1]=base;\n   i=i+1;\n  }\n  return R_OK,start,pos,base,st[1]-base;'),
    ('atomic keeps nodes on NEED_MORE',
     '  if k==K_ATOMIC && t==R_NEED { return t,s,e,st[1],0; }',
     '  if k==K_ATOMIC && t==R_NEED { return t,s,e,rb,rc; }'),
    ('a failing rule still creates a node',
     '  if t==R_FAIL { return t,s,e,st[1],0; }\n  let nid=st[0];',
     '  let nid=st[0];'),
    ('the dump indents by one space',
     ' while two<indent*2 { p=append_c(out,p,\' \'); two=two+1; }',
     ' while two<indent { p=append_c(out,p,\' \'); two=two+1; }'),
    ('the dump omits the rule name',
     ' if a_rule[id]>=0 {',
     ' if a_rule[id]>=999 {'),
    ('the dump omits the tag name',
     ' if a_tag[id]>=0 {',
     ' if a_tag[id]>=999 {'),
    ('the dump prints no text',
     ' p=append_s(out,p,input,a_toff[id],a_tlen[id]);',
     ' p=append_s(out,p,input,0,0);'),
    ('the dump skips children',
     '  dump_node(achild[cb+z],indent+1,input,strs,sdir,a_rule,a_tag,a_start,a_end,a_toff,a_tlen,a_cs,a_cc,achild,out,st);',
     '  dump_node(achild[cb+z],indent,input,strs,sdir,a_rule,a_tag,a_start,a_end,a_toff,a_tlen,a_cs,a_cc,achild,out,st);'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG484_PEGAST')]
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
