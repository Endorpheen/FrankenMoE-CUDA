from pathlib import Path
import subprocess,json,shutil,hashlib,os,runpy
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');M=A/'source/graph-metadata';D=A/'source/graph-result';B=A/'build/graph-metadata';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');O=A/'helpers/omo5';B.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=150);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-5000:]);raise SystemExit(p.returncode)
 return p
M.mkdir(exist_ok=True)
for f in (O/'worker-4/graph-metadata').glob('*'):
 if f.is_file() and f.suffix in {'.py','.cpp','.h','.json','.md'}:shutil.copyfile(f,M/f.name)
v=runpy.run_path(str(M/'make_patched.py'));source=(D/'graph.cpp').read_text();a=source.index(v['CTOR_START']);b=source.index(v['CTOR_END'],a)+len(v['CTOR_END']);old=source[a:b];source=source[:a]+v['GOOSE_BRANCH']+old+'\n#endif'+source[b:];source=source.replace('#include "llama-graph.h"','#include "llama-graph.h"\n#include "graph-meta.h"',1);cpp=M/'graph.cpp';cpp.write_text(source)
H=M/'native/src';H.mkdir(parents=True,exist_ok=True)
for f in (U/'src').rglob('*.h'):
 q=H/f.relative_to(U/'src');q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,q)
h=(U/'src/llama-graph.h').read_text();assert h.count(v['H_FWD_ANCHOR'])==1 and h.count(v['H_CTOR_ANCHOR'])==1;h=h.replace(v['H_FWD_ANCHOR'],v['H_FWD_NEW'],1).replace(v['H_CTOR_ANCHOR'],v['H_CTOR_NEW'],1);(H/'llama-graph.h').write_text(h)
rows=json.loads((A/'build/graph-result/commands.json').read_text());r=next(x for x in rows if x['label']=='result-graph-build');args=r['argv'].copy();args[args.index('-c')+1]=str(cpp);args[args.index('-o')+1]=str(B/'graph.o');args[1:1]=['-I'+str(H),'-I'+str(D)];run('metadata-graph-build',args,Path(r['cwd']))
rows=json.loads((A/'build/batch-reserve-fix/commands.json').read_text());r=next(x for x in rows if x['label']=='reserve-fix-cohort-link');args=r['argv'].copy();old=str(A/'build/graph-result/graph.o');assert args.count(old)==1;args=[str(B/'graph.o') if x==old else x for x in args];fingerprint=hashlib.sha256(b''.join((Path(x) if Path(x).is_absolute() else Path(r['cwd'])/x).read_bytes() for x in args if x.endswith('.o'))).hexdigest()[:12];out=R/('libllama-apex-metadata-'+fingerprint+'.so.0.7.0');args[args.index('-o')+1]=str(out);run('metadata-cohort-link',args,Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing '+str(q))
  q.symlink_to(out if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
incs=['-I'+str(H),'-I'+str(D),'-I'+str(U/'src'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')];run('metadata-real-ctor-build',['/usr/bin/g++-13','-std=c++17','-O1',*incs,str(M/'ctor-corpus.cpp'),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lggml-cpu','-lm','-o',str(B/'ctor-corpus')]);env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R);p=run('metadata-real-ctor',[str(B/'ctor-corpus')],env=env);assert 'PASS apex_like' in p.stdout and 'PASS gqa_nextn0_warmup' in p.stdout
run('metadata-combined-public',[str(A/'build/graph-result/public-fixture')],env=env)
summary={'status':'ACCEPTED_REAL_GRAPH_CONTEXT_METADATA','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'source_worker':4,'reused_worker':7,'real_ctor_stdout':p.stdout,'layout_changed':False,'compiler_changed':False,'model_requests':0,'production_changed':False,'rollback':'FG153_NATIVE_ROLLBACK or previous immutable runtime library','pending':'Whole Batch A actual Apex gate'};(A/'results/accepted-graph-metadata.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('Accepted Goose-derived metadata in the real graph constructor,with result/input hooks retained.')
