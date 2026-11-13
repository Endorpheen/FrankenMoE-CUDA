#!/usr/bin/env python3
from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-13-491-p7-09-lazy';S=E/'source/peggram4.goose';O=S.read_text()
M=[('trigger names are not sorted','    if !less { idxs[j2]=prev; j2=j2-1; } else { stop=true; }','    if false { idxs[j2]=prev; j2=j2-1; } else { stop=true; }'),
   ('trigger names join with a comma','   if q>0 { p=append_s(out,p," | ",0,3); }','   if q>0 { p=append_s(out,p,", ",0,2); }'),
   ('trigger reachability is skipped','    reach(rid[zt],marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st);','    reach(rid[zt],marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st);\n    marks[zt]=0;'),
   ('lazy is ignored',' if lazy==0 {\n  reach(root,marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st);\n } else {',' if true {\n  reach(root,marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st);\n } else {'),
   ('the lazy root is the arena root text',' if lazy==0 {\n  let a,b=to_gbnf(root,out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,schema_add,mname,mval,mlen);\n  co=a; cl=b;\n } else {',' if true {\n  let a,b=to_gbnf(root,out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,schema_add,mname,mval,mlen);\n  co=a; cl=b;\n } else {')]
def run():
 try: r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=180,errors='replace')
 except subprocess.TimeoutExpired: return 1,'timeout'
 line=[x for x in r.stdout.splitlines() if x.startswith('FG491_PEGLAZY')]
 if line: return r.returncode,line[-1]
 return None,'build failed'
c=0
try:
 for l,o,n in M:
  if o not in O: print('[skip]',l); continue
  S.write_text(O.replace(o,n,1)); rc,line=run()
  if rc is None: print('[build-fail]',l); continue
  ok=rc!=0; c+=ok; print(f'[{"caught" if ok else "MISSED"}] {l}: {line}')
finally: S.write_text(O); run()
print(f'mutations {c}/{len(M)}')
