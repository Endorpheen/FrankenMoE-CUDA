#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-475-p7-09-visit'
SRC = E / 'source/visit.goose'
ORIG = SRC.read_text()

M = [
    ('required chain loses its comma',
     '   if have_req { p=append_s(out,p," \\",\\" space ",0,11); }',
     '   if have_req { p=append_s(out,p," ",0,1); }'),
    ('optional block loses its opener',
     '  p=append_s(out,p," (",0,2);',
     '  p=append_s(out,p,"",0,0);'),
    ('required-optional opener loses the comma',
     '  if have_req { p=append_s(out,p," \\",\\" space ( ",0,13); }',
     '  if have_req { p=append_s(out,p," ( ",0,3); }'),
    ('star branch uses ? instead of *',
     '   p=append_s(abuf,p," )*",0,3);',
     '   p=append_s(abuf,p," )?",0,3);'),
    ('star-first branch uses ? instead of *',
     "  if is_star { p=append_c(abuf,p,'*'); } else { p=append_c(abuf,p,'?'); }",
     "  p=append_c(abuf,p,'?');"),
    ('rest rule name separator becomes X',
     '  let no,nl=build_kv(abuf,ast,name,keyname,starflag,"-rest");',
     '  let no,nl=build_kv(abuf,ast,name,keyname,starflag,"Xrest");'),
    ('rest rules are not deduped',
     '  if !dup {',
     '  if true {'),
    ('kv link loses its space',
     "  p=append_c(abuf,p,' ');\n  p=append_s(abuf,p,abuf,rname_off,rname_len);",
     '  p=append_s(abuf,p,abuf,rname_off,rname_len);'),
    ('empty object is a bare pair of braces',
     '  return append_s(out,0,"\\"{\\" space \\"}\\"",0,13);',
     '  return append_s(out,0,"\\"{}\\"",0,4);'),
    ('tuple comma loses the spaces',
     '  if i>0 { p=append_s(out,p," \\",\\" space ",0,11); }',
     '  if i>0 { p=append_s(out,p,"\\",\\"",0,3); }'),
    ('array brackets are swapped',
     ' var p=append_s(out,0,"\\"[\\" space ",0,10);',
     ' var p=append_s(out,0,"\\"]\\" space ",0,10);'),
    ('array keeps a negative maximum',
     ' var mmx:i64=mx;\n if mmx<0 { mmx=INT_MAX; }',
     ' var mmx:i64=mx;'),
    ('empty-separator plus becomes star',
     "  if mn==1 && !has_max {\n   p=append_c(abuf,p,'+');",
     "  if mn==1 && !has_max {\n   p=append_c(abuf,p,'*');"),
    ('empty-separator star becomes plus',
     "  if mn==0 && !has_max {\n   p=append_c(abuf,p,'*');",
     "  if mn==0 && !has_max {\n   p=append_c(abuf,p,'+');"),
    ('union separator is a comma',
     '  if i>0 { p=append_s(out,p," | ",0,3); }',
     '  if i>0 { p=append_s(out,p," , ",0,3); }'),
    ('kv separator loses its quotes',
     ' let sep: const u8[:] = " space \\":\\" space ";',
     ' let sep: const u8[:] = " space : space ";'),
    ('optional entries are reversed',
     '  if req[i]==0 { opt[oc]=i; oc=oc+1; }',
     '  if req[i]==0 { opt[n-1-i]=i; oc=oc+1; }'),
    ('empty object check also catches additional',
     ' if n==0 && add_kind==0 {',
     ' if n==0 && add_kind!=0 {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=120, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG475_VISIT')]
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
