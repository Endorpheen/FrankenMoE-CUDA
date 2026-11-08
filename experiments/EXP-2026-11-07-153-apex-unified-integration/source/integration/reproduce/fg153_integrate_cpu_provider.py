from pathlib import Path
import subprocess,json,shutil,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/cpu-provider';B=A/'build/cpu-provider';R=A/'variants/cpu-backend/run';P=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');N=D/'native-cpu';B.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=150);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-5500:]);raise SystemExit(p.returncode)
 return p
for f in (P/'source/cpu-backend').rglob('*'):
 if f.is_file() and f.suffix in {'.c','.cpp','.h','.goose'} and 'reproduce' not in f.parts:
  q=N/f.relative_to(P/'source/cpu-backend');q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,q)
p=N/'native-bridge.cpp';s=p.read_text();needle='extern "C" uint8_t fg143_try(const ggml_compute_params*params,ggml_tensor*t){';assert s.count(needle)==1;s=s.replace(needle,'extern "C" uint8_t fg006_try(const ggml_compute_params*,ggml_tensor*);\n'+needle+'\n if(fg006_try(params,t))return 1;');p.write_text(s)
p=N/'ggml-cpu-integrated.c';s=p.read_text();needle='extern size_t fg143_extra_workspace(const struct ggml_tensor*,int);';assert s.count(needle)==1;s=s.replace(needle,needle+'\nextern size_t fg006_extra_workspace(const struct ggml_tensor*,int);');old='size_t size=fg143_extra_workspace(fg143_control_node(x,index),(int)tasks);';assert s.count(old)==1;s=s.replace(old,'size_t size=fg006_extra_workspace(fg143_control_node(x,index),(int)tasks);if(!size)size=fg143_extra_workspace(fg143_control_node(x,index),(int)tasks);');needle='cur=MAX(cur,fg143_extra_workspace(node,n_tasks));';assert s.count(needle)==1;s=s.replace(needle,needle+'\n        cur=MAX(cur,fg006_extra_workspace(node,n_threads));');p.write_text(s)
p=N/'control/kernel.goose';s=p.read_text();old='goose_work(c,n,count)';assert s.count(old)==1;s=s.replace(old,'goose_work(c,n,threads)');p.write_text(s)
producer=A/'compiler/stage3-f849233b999c';run('provider-emit',[str(producer),'-o',str(D/'provider-generated.c'),str(D/'provider.goose')],D);run('provider-control-emit',[str(producer),'-o',str(B/'control.c'),str(p)])
p=N/'control/unit.c';s=p.read_text().replace('"../../../generated/cpu-control.c"','"'+str(B/'control.c')+'"');p.write_text(s)
rows=json.loads((P/'variants/cpu-backend/build/commands.json').read_text());core=next(r for r in reversed(rows) if r['label']=='core' and r['exit_code']==0);args=core['argv'].copy();args[args.index('-c')+1]=str(N/'ggml-cpu-integrated.c');args[args.index('-o')+1]=str(B/'core.o');run('provider-core',args,Path(core['cwd']))
incs=['-I'+str(N),'-I'+str(D),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src'),'-I'+str(U/'ggml/src/ggml-cpu')]
run('provider-native-bridge',['/usr/bin/g++-13','-std=c++17','-O3','-march=native','-fPIC','-fno-strict-aliasing','-ffp-contract=off',*incs,'-c',str(N/'native-bridge.cpp'),'-o',str(B/'native-bridge.o')])
for source,name in [(D/'provider-unit.c','unit'),(D/'provider-dispatch.c','dispatch'),(N/'control/unit.c','control')]:
 run('provider-'+name,['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O2','-march=native','-fPIC','-fexceptions','-fno-strict-aliasing','-ffp-contract=off','-Werror=implicit-function-declaration',*incs,'-c',str(source),'-o',str(B/(name+'.o'))])
r=next(r for r in reversed(rows) if r['label']=='link-cpu' and r['exit_code']==0);args=r['argv'].copy();repl={str(P/'variants/cpu-backend/build/core.o'):str(B/'core.o'),str(P/'variants/cpu-backend/build/cpu-bridge.o'):str(B/'native-bridge.o'),str(P/'variants/cpu-backend/build/control.o'):str(B/'control.o')};assert all(args.count(k)==1 for k in repl);args=[repl.get(x,x) for x in args];args=[str(R/'libggml-base.so') if 'libggml-base.so' in x and not x.startswith('-') else x for x in args];args += [str(B/'unit.o'),str(B/'dispatch.o'),'-Wl,-z,defs'];out=R/('libggml-cpu-provider-'+hashlib.sha256(b''.join((B/f).read_bytes() for f in ['core.o','native-bridge.o','control.o','unit.o','dispatch.o'])).hexdigest()[:12]+'.so.0.27.0');args[args.index('-o')+1]=str(out);run('provider-cohort-link',args,Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing '+str(q))
  q.symlink_to(out if f.name in ['libggml-cpu.so','libggml-cpu.so.0','libggml-cpu.so.0.27.0'] else f.resolve())
fixture=A/'source/cpu-provider-integration/backend-integration-fixture.c';run('provider-backend-fixture-build',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O1',*incs,str(fixture),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lggml-cpu','-lggml-base','-lm','-lpthread','-o',str(B/'backend-fixture')])
arms=[]
for flag in ['0','1']:
 env=os.environ.copy();env.update(LD_LIBRARY_PATH=str(T)+':'+str(R),GGML_GOOSE_CPU_PROVIDER=flag,GGML_GOOSE_CPU_BACKEND=flag,GGML_GOOSE_CPU_CONTROL=flag,GGML_GOOSE_CPU_EXECUTOR=flag)
 dest=B/('backend-'+flag+'.jsonl');p=run('provider-backend-fixture-'+flag,[str(B/'backend-fixture'),str(dest)],env=env);items=[json.loads(s) for s in dest.read_text().splitlines()];arms.append({x['name']:x['hash'] for x in items if 'name' in x});assert 'BACKEND_CORPUS PASS' in p.stdout
assert arms[0]==arms[1],[(k,arms[0].get(k),v) for k,v in arms[1].items() if arms[0].get(k)!=v]
assert 'FG006_GOOSE' in (A/'results/provider-backend-fixture-1.stderr').read_text()
summary={'status':'ACCEPTED_CPU_PROVIDER_LOCAL_COHORT','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'source_worker':6,'normal_backend_cases':len(arms[0]),'source_routing':'actual changed core,bridge,Goose control objects;no preload/interposition','minor_parent_fix':'Goose workspace passes actual worker count instead of op n_tasks;no global thread counter or private layout read','model_requests':0,'production_changed':False,'rollback':'GGML_GOOSE_CPU_PROVIDER=0','pending':'APX013 whole closure and Batch A Apex gate'};(A/'results/accepted-cpu-provider.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libggml-cpu.so','libggml-cpu.so.0','libggml-cpu.so.0.27.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('Accepted CPU numerical/provider actual source integration and native differential.')
