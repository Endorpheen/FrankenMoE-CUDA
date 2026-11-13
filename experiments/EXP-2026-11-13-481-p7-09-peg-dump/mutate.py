#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-481-p7-09-peg-dump'
SRC = E / 'source/pegdump.goose'
ORIG = SRC.read_text()

M = [
    ('visited ids are never recorded',
     ' vis[st[0]]=id;\n st[0]=st[0]+1;',
     ' st[0]=st[0]+1;'),
    ('the cycle check is disabled',
     ' if vis_has(vis,st[0],id) {',
     ' if false {'),
    ('Tag keeps the outer visited set',
     '  let save=st[0];\n  st[0]=0;',
     '  let save=st[0];'),
    ('Tag does not restore the outer visited',
     '  st[0]=save;\n  p=append_c(out,p,\')\');',
     '  p=append_c(out,p,\')\');'),
    ('unbounded repetitions print a number',
     '  if mx[id]==-1 {\n   p=append_s(out,p,"unbounded",0,9);',
     '  if mx[id]==-2 {\n   p=append_s(out,p,"unbounded",0,9);'),
    ('sequence parts lose the space',
     '   if i>0 { p=append_s(out,p,", ",0,2); }',
     '   if i>0 { p=append_s(out,p,",",0,1); }'),
    ('choices are called sequences',
     '  if k==4 { p=append_s(out,p,"Sequence(",0,9); } else { p=append_s(out,p,"Choice(",0,7); }',
     '  if k==4 { p=append_s(out,p,"Sequence(",0,9); } else { p=append_s(out,p,"Sequence(",0,9); }'),
    ('string delimiters lose their parentheses',
     '  p=append_s(out,p,"String(",0,7);',
     '  p=append_s(out,p,"String",0,6);'),
    ('until delimiters lose their space',
     '   if i2>0 { p=append_s(out,p," | ",0,3); }',
     '   if i2>0 { p=append_s(out,p,"|",0,1); }'),
    ('ac delimiters use a comma',
     '   if i3>0 { p=append_s(out,p," | ",0,3); }',
     '   if i3>0 { p=append_s(out,p,", ",0,2); }'),
    ('chars unbounded prints a number',
     '  if mx[id]==-1 {\n   p=append_s(out,p,"unbounded",0,9);\n  } else {\n   p=append_dec(out,p,mx[id]);\n  }\n  p=append_c(out,p,\')\');\n } else if k==12 {',
     '  if mx[id]==-2 {\n   p=append_s(out,p,"unbounded",0,9);\n  } else {\n   p=append_dec(out,p,mx[id]);\n  }\n  p=append_c(out,p,\')\');\n } else if k==12 {'),
    ('rule child loses its comma',
     '  p=append_s(out,p,", ",0,2);\n  st[1]=p;\n  dump_node(children[cfirst[id]],kind,cfirst,ccount,children,sidx,dstart,dcount,delims,mn,mx,strs,sdir,out,vis,st);\n  p=st[1];\n  p=append_c(out,p,\')\');\n } else if k==15 {',
     '  st[1]=p;\n  dump_node(children[cfirst[id]],kind,cfirst,ccount,children,sidx,dstart,dcount,delims,mn,mx,strs,sdir,out,vis,st);\n  p=st[1];\n  p=append_c(out,p,\')\');\n } else if k==15 {'),
    ('epsilon is misspelled',
     '  p=append_s(out,p,"Epsilon",0,7);',
     '  p=append_s(out,p,"Epsilom",0,7);'),
    ('refs lose their name',
     '  p=append_s(out,p,"Ref(",0,4);\n  let s=sget(strs,sdir,sidx[id]);\n  p=append_s(out,p,s,0,s.len);',
     '  p=append_s(out,p,"Ref(",0,4);'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG481_PEG')]
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
