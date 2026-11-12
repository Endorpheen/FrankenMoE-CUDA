from pathlib import Path
import json,subprocess,os,shlex,sys
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');san='--sanitize' in sys.argv;v=A/'variants'/('sanitized' if san else 'cpu-backend');b=v/'build';run=v/'run';out=A/'results'/('sanitized' if san else 'release');out.mkdir(exist_ok=True);commands=[]
def execute(label,argv,env=None):
 p=subprocess.run(argv,capture_output=True,text=True,env=env,timeout=180);(out/(label+'.log')).write_text(p.stdout+p.stderr);commands.append(dict(label=label,argv=argv,exit_code=p.returncode));(out/'batch-commands.json').write_text(json.dumps(commands,indent=2)+'\n');print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-3000:]);raise SystemExit(p.returncode)
 return p
flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-O1'] if san else ['-O2']
base=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');includes=['-I'+str(base/x) for x in ['src','include','ggml/include','ggml/src']]
cmd=['/usr/bin/g++-13',*flags,*includes,'-I'+str(A/'source/expert-cache'),str(A/'source/expert-cache/fixture.cpp'),str(b/'cache-goose.o'),str(b/'budget-services.o'),'-L'+str(run),'-Wl,-rpath,'+str(run),'-lggml-base','-o',str(b/'cache-fixture')];execute('compile-cache',cmd)
env=dict(os.environ,LD_LIBRARY_PATH=str(run),ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');env.pop('LD_PRELOAD',None)
p=execute('cache-differential',[str(b/'cache-fixture')],env);report=json.loads(p.stdout);(out/'cache-summary.json').write_text(json.dumps(report,indent=2)+'\n')

execute('compile-registry',['/usr/bin/gcc-13',*flags,str(A/'source/cpu-backend/registry/fixture.c'),str(b/'registry.o'),'-o',str(b/'registry-fixture')])
p=execute('registry-policy',[str(b/'registry-fixture')],env);(out/'registry-summary.json').write_text(json.dumps(json.loads(p.stdout),indent=2)+'\n')
