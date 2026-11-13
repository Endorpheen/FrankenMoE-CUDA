#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-495-p7-09-schema-introspect'
SRC = E / 'source/schemaint.goose'
ORIG = SRC.read_text()

M = [
    ('any loses the object type', 'fn all_types()->i64 { return 127; }', 'fn all_types()->i64 { return 63; }'),
    ('number is not an integer', ' if k==K_NUMBER { return bit(T_NUMBER)|bit(T_INTEGER); }', ' if k==K_NUMBER { return bit(T_NUMBER); }'),
    ('a ref cycle is not guarded', '  if vis_find(vis,st[0],t)>=0 { return 0; }\n  vis[st[0]]=t;', '  vis[st[0]]=t;'),
    ('the visited entry is not popped', '  st[0]=st[0]-1;\n  return m;\n }', '  return m;\n }'),
    ('allOf starts from nothing', '  var m:i64=all_types();', '  var m:i64=0;'),
    ('anyOf intersects', '   m=m|vt(nchild[ncf[id]+i]', '   m=m&vt(nchild[ncf[id]+i]'),
    ('enum contributes nothing', '   m=m|bit(eval_t[nef[id]+i]);', '   m=m|0;'),
    ('const uses the wrong type', ' if k==K_CONST { return bit(ctype[id]); }', ' if k==K_CONST { return bit(T_STRING); }'),
    ('may_be_string ignores any children in allOf', '   if nkind[c]!=K_ANY {', '   if true {'),
    ('may_be_string accepts every allOf child', '    if mbs(c,nkind,ncf,ncc,nchild,ntarget,ctype,nef,nec,eval_t,vis,st)==0 { return 0; }', '    if false { return 0; }'),
    ('may_be_string looks at one enum value', '   if eval_t[nef[id]+i]==T_STRING { return 1; }', '   if i==0 && eval_t[nef[id]]==T_STRING { return 1; }'),
    ('may_be_string ignores the ref cycle guard', '  if vis_find(vis,st[0],t)>=0 { return 0; }\n  vis[st[0]]=t;\n  st[0]=st[0]+1;\n  let r=mbs', '  vis[st[0]]=t;\n  st[0]=st[0]+1;\n  let r=mbs'),
    ('kind_name spells anyOf differently', ' else if k==2 { n="anyOf"; }', ' else if k==2 { n="anyof"; }'),
    ('type_name spells integer as int', ' else if t==3 { n="integer"; }', ' else if t==3 { n="int"; }'),
    ('a tuple reports integer', ' if k==K_ARRAY || k==K_TUPLE { return bit(T_ARRAY); }', ' if k==K_ARRAY { return bit(T_ARRAY); }\n if k==K_TUPLE { return bit(T_INTEGER); }'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG495_SCHEMAINT')]
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
