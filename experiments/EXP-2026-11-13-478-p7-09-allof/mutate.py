#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-478-p7-09-allof'
SRC = E / 'source/allof.goose'
ORIG = SRC.read_text()

M = [
    ('anyOf alternatives are marked required',
     '    walk(citems[base+j],0,ckind,cfirst,ccount,citems,pnames,pdir,vdumps,vdir,out_props,out_req,tinfo,abuf,st);',
     '    walk(citems[base+j],1,ckind,cfirst,ccount,citems,pnames,pdir,vdumps,vdir,out_props,out_req,tinfo,abuf,st);'),
    ('anyOf alternatives are skipped',
     '    walk(citems[base+j],0,ckind,cfirst,ccount,citems,pnames,pdir,vdumps,vdir,out_props,out_req,tinfo,abuf,st);',
     '    j=j+0;'),
    ('refs drop the required flag',
     '  return walk(target,req,ckind,cfirst,ccount,citems,pnames,pdir,vdumps,vdir,out_props,out_req,tinfo,abuf,st);',
     '  return walk(target,0,ckind,cfirst,ccount,citems,pnames,pdir,vdumps,vdir,out_props,out_req,tinfo,abuf,st);'),
    ('refs ignore their target',
     '  return walk(target,req,ckind,cfirst,ccount,citems,pnames,pdir,vdumps,vdir,out_props,out_req,tinfo,abuf,st);',
     '  return 0;'),
    ('required is no longer a name set',
     '  if out_req[a]==0 {',
     '  if out_req[a]==99 {'),
    ('every occurrence becomes required',
     '    if out_req[b]!=0 {',
     '    if out_req[b]>=0 {'),
    ('enum count compares with !=',
     '  if tinfo[r*4+3]==nchildren {',
     '  if tinfo[r*4+3]!=nchildren {'),
    ('enum count needs one extra child',
     '  if tinfo[r*4+3]==nchildren {',
     '  if tinfo[r*4+3]==nchildren+1 {'),
    ('intersection order is not sorted',
     '   if less_bytes(abuf[cur_off..cur_off+cur_len],abuf[po..po+pl]) {',
     '   if false {'),
    ('intersection sorts descending',
     '   if less_bytes(abuf[cur_off..cur_off+cur_len],abuf[po..po+pl]) {',
     '   if less_bytes(abuf[po..po+pl],abuf[cur_off..cur_off+cur_len]) {'),
    ('identical enum texts are not aggregated',
     '   if found>=0 {',
     '   if found>=99 {'),
    ('properties are prepended',
     '   let oi=st[0];\n   out_props[oi]=pi;\n   out_req[oi]=req;\n   st[0]=oi+1;',
     '   let oi=st[0];\n   var mv:i64=oi;\n   while mv>0 { out_props[mv]=out_props[mv-1]; out_req[mv]=out_req[mv-1]; mv=mv-1; }\n   out_props[0]=pi;\n   out_req[0]=req;\n   st[0]=oi+1;'),
    ('an empty intersection still returns a value',
     ' if n>0 { st[2]=0; } else { st[2]=1; }',
     ' if st[3]>0 { st[2]=0; } else { st[2]=1; }'),
    ('byte ordering is reversed',
     '  if a[i]<b[i] { return true; }',
     '  if a[i]>b[i] { return true; }'),
    ('top-level walk ignores the top list',
     '  let ci=top[i];',
     '  let ci=i;'),
    ('the object walk keeps one occurrence per name',
     '   st[0]=oi+1;',
     '   if oi==0 { st[0]=oi+1; } else { st[0]=oi; }'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG478_ALLOF')]
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
