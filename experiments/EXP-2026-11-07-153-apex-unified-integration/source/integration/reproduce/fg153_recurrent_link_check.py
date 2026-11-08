from pathlib import Path
import subprocess,json,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');B=A/'build/recurrent-memory';R=A/'variants/cpu-backend/run';T=B/'run';T.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=120);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'integration-commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-6000:]);raise SystemExit(p.returncode)
 return p
rows=json.loads((A/'build/hybrid-inputs/commands.json').read_text());recipe=next(x for x in rows if x['label']=='hybrid-input-link');args=recipe['argv'].copy();fingerprint=hashlib.sha256(b''.join(Path(x).read_bytes() for x in json.loads((B/'objects.json').read_text())['objects'])).hexdigest()[:12];out=R/('libllama-apex-recurrent-'+fingerprint+'.so.0.7.0');args[args.index('-o')+1]=str(out)
old='src/CMakeFiles/llama.dir/llama-memory-recurrent.cpp.o';assert args.count(old)==1;args=[str(B/'llama-memory-recurrent.o') if x==old else x for x in args];args += [x for x in json.loads((B/'objects.json').read_text())['objects'] if x!=str(B/'llama-memory-recurrent.o')]
if str(B/'hybrid-caller.o') in args:
 args.remove(str(A/'build/hybrid-memory/hybrid.o'))
inputs=[(Path(x) if Path(x).is_absolute() else Path(recipe['cwd'])/x) for x in args if x.endswith('.o')]
reused=out.exists() and out.stat().st_mtime>=max(x.stat().st_mtime for x in inputs)
if not reused:run('recurrent-integrated-link',args,Path(recipe['cwd']))
else:print('reused unchanged linked recurrent library',flush=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing to replace '+str(q))
  q.symlink_to(out if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R)
if not reused:run('recurrent-integrated-dlopen',['python3','-c','import ctypes,os;ctypes.CDLL('+repr(str(out))+',mode=os.RTLD_NOW);print("PASS actual merged recurrent DSO symbol resolution")'],env=env)
run('recurrent-actual-fixture-link',['/usr/bin/g++-13',str(B/'fixture.o'),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lm','-o',str(B/'fixture')])
p=run('recurrent-actual-fixture',[str(B/'fixture')],env=env)
summary={'status':'PASS_RECURRENT_NATIVE_GOOSE_CONCRETE_ABI_LOCAL','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'fixture_stdout':p.stdout,'witness':p.stderr,'model_requests':0,'production_changed':False,'integration':'NOT_ACTIVATED_PENDING_REAL_HYBRID_COHORT','residuals':['default constructor resource allocation pending Goose lifecycle hook','normal hybrid direct concrete-context construction needs owned activation','native container/ABI storage retained']}
(A/'results/recurrent-actual-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print('Local concrete recurrent differential passed; actual hybrid combined gate still required.')
