from pathlib import Path
import subprocess,json,shutil,re,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/context-control';B=A/'build/context-control';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');O=A/'helpers/omo5';B.mkdir(exist_ok=True);(D/'gen').mkdir(exist_ok=True);H=D/'native/src';H.mkdir(parents=True,exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=180);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-6000:]);raise SystemExit(p.returncode)
 return p
for f in (U/'src').glob('*.h'):shutil.copyfile(f,H/f.name)
h=(O/'inputs/active-llama-context.h').read_text();assert h.count('struct llama_context {')==1;h=h.replace('struct llama_context {','struct llama_context {\n    friend struct fg107_context_access;');(H/'llama-context.h').write_text(h)
p=D/'make_patched.py';s=p.read_text().replace('Path("/inputs/active-llama-context.cpp")','Path('+repr(str(O/'inputs/active-llama-context.cpp'))+')').replace('Path("/inputs/active-llama-context.h")','Path('+repr(str(O/'inputs/active-llama-context.h'))+')');p.write_text(s);run('context-source-replay',['python3',str(p)])
for module in ['execution','resource','policy']:
 run('context-emit-'+module,[str(A/'compiler/stage3-f849233b999c'),'-o',str(D/'gen'/('context-'+module+'.c')),str(D/('context-'+module+'.goose'))])
 p=D/('unit-'+('exec' if module=='execution' else module)+'.c');s=p.read_text();gen=(D/'gen'/('context-'+module+'.c')).read_text();hot=gen[:gen.rfind('static void gg_function_0(')];assert not re.search(r'gg_stacks\[[^\]]+\]\.',hot)
 for symbol in set(re.findall(r'gg_function_\d+',s)):assert re.search(r'\b'+symbol+r'\(',gen)
incs=['-I'+str(D),'-I'+str(H),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')];objects=[]
for name in ['exec','resource','policy']:
 obj=B/('unit-'+name+'.o');run('context-unit-'+name,['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O2','-fPIC','-fexceptions','-fno-strict-aliasing','-Werror=implicit-function-declaration',*incs,'-c',str(D/('unit-'+name+'.c')),'-o',str(obj)]);objects.append(obj)
for source,name in [('patched/llama-context.cpp','context'),('context-services.cpp','services'),('fg107-compat.cpp','compat')]:
 obj=B/(name+'.o');run('context-cpp-'+name,['/usr/bin/g++-13','-std=c++17','-O2','-fPIC','-fno-strict-aliasing','-DLLAMA_BUILD','-DLLAMA_SHARED','-DGGML_USE_CUDA',*incs,'-c',str(D/source),'-o',str(obj)]);objects.append(obj)
rows=json.loads((A/'build/batch-allocr/commands.json').read_text());r=next(x for x in rows if x['label']=='batch-cohort-link');args=r['argv'].copy();old='/home/end0/AI/LlamaServer/exp107-expert-overlap/build/context.o';assert args.count(old)==1;args=[str(B/'context.o') if x==old else x for x in args];args += [str(x) for x in objects if x.name!='context.o'];out=R/('libllama-apex-context-'+hashlib.sha256(b''.join(x.read_bytes() for x in objects)).hexdigest()[:12]+'.so.0.7.0');args[args.index('-o')+1]=str(out);run('context-cohort-link',args,Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing '+str(q))
  q.symlink_to(out if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R)
run('context-dlopen',['python3','-c','import ctypes,os;ctypes.CDLL('+repr(str(out))+',mode=os.RTLD_NOW);print("PASS actual context/shared cohort symbols")'],env=env)
for module,unit in [('trace','exec'),('resource','resource'),('policy','policy')]:
 source=D/('context-trace-driver.cpp' if module=='trace' else 'context-'+module+'-driver.cpp');binary=B/(module+'-driver');run('context-fixture-build-'+module,['/usr/bin/g++-13','-std=c++17','-O1','-I'+str(D),str(source),str(B/('unit-'+unit+'.o')),'-lm','-o',str(binary)]);run('context-fixture-'+module,[str(binary)])
summary={'status':'LINKED_CONTEXT_COHORT_LOCAL_DRIVERS_PASS','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'source_worker':4,'model_requests':0,'production_changed':False,'activation':'NOT_ACTIVATED pending real context functional fixture / final Batch A gate','residuals':['on-device state IO integration awaits stream-io bridge','optional pin/expert pipeline capabilities absent in header snapshot','full Apex constructor/decode gate not run'],'rollback':'Original EXP107 context object retained;FG107_NATIVE_ROLLBACK build'};(A/'results/context-cohort-linked.json').write_text(json.dumps(summary,indent=2)+'\n');print('Context cohort linked; no activation until actual context correctness gate.')
