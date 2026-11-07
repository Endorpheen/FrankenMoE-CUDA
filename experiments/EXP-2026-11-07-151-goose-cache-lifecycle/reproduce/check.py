from pathlib import Path
import subprocess,json,os
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107');P=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');base=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');b=A/'build';out=A/'results';records=[]
def run(label,argv,env=None):
 r=subprocess.run(argv,capture_output=True,text=True,env=env,timeout=180);(out/(label+'.log')).write_text(r.stdout+r.stderr);records.append(dict(label=label,argv=argv,exit_code=r.returncode));(out/'local-fixture-commands.json').write_text(json.dumps(records,indent=2)+'\n')
 if r.returncode:print((r.stdout+r.stderr)[-2500:]);raise SystemExit(r.returncode)
 return r
inc=['-I'+str(A/'source/expert-cache')]+['-I'+str(base/p) for p in ['src','include','ggml/include','ggml/src','ggml/src/ggml-cpu']]
run('compile-lifecycle-fixture',['/usr/bin/g++-13','-std=c++17','-O2','-fno-strict-aliasing',*inc,str(A/'source/expert-cache/lifecycle-fixture.cpp'),str(b/'lifecycle.o'),str(P/'variants/cpu-backend/build/cache-goose.o'),str(P/'variants/cpu-backend/build/budget-services.o'),'-L'+str(P/'variants/cpu-backend/run'),'-Wl,-rpath,'+str(P/'variants/cpu-backend/run'),'-lggml-cpu','-lggml','-lggml-base','-l:libllama.so.0','-o',str(b/'lifecycle-fixture')])
env=dict(os.environ,LD_LIBRARY_PATH=str(P/'variants/cpu-backend/run'),GGML_EXPERT_HOTSTORE='0');env.pop('LD_PRELOAD',None)
r=run('lifecycle-local-functional',[str(b/'lifecycle-fixture')],env);summary=json.loads(r.stdout);(out/'lifecycle-local-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print('Local native/Goose cache controller check passed;no model or broad suite executed')
