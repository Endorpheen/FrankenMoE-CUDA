from pathlib import Path
import json,os,shutil,subprocess,sys,shlex
old=Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107');a=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');san='--sanitize' in sys.argv;mode='sanitized' if san else 'cpu-backend';b=a/'variants'/mode/'build';run=a/'variants'/mode/'run';b.mkdir(parents=True,exist_ok=True);run.mkdir(exist_ok=True)
oldb=old/'variants'/mode/'build'
if not oldb.exists():raise SystemExit('Missing qualified inherited variant')
for p in oldb.glob('*.o'):
 if not(b/p.name).exists():shutil.copy2(p,b/p.name)
for p in (old/'variants'/mode/'run').iterdir():
 if p.is_file() and not(run/p.name).exists():shutil.copy2(p,run/p.name)
records=json.loads((b/"commands.json").read_text()) if "--resume" in sys.argv and (b/"commands.json").exists() else []
completed={r["label"] for r in records if r["exit_code"]==0} - {"core","registry","backend-cpp","cache-goose","cache-cpp","link-cpu","link-llama"}
def run_cmd(label,argv,cwd=a):
 if "--resume" in sys.argv and label in completed and not label.startswith("generate-"):print(label,"reused",flush=True);return
 p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,timeout=240);(b/(label+'.log')).write_text(p.stdout+p.stderr);records.append(dict(label=label,argv=argv,cwd=str(cwd),exit_code=p.returncode));(b/'commands.json').write_text(json.dumps(records,indent=2)+'\n');print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-3500:]);raise SystemExit(p.returncode)
def mapped(x):return x.replace(str(old/'source/cpu-backend'),str(a/'source/cpu-backend')).replace(str(old/'generated'),str(a/'generated')).replace(str(old/'variants'/mode),str(a/'variants'/mode))
def inst(cmd):return cmd+(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-O1'] if san else [])
for out,src in [('cpu-registry.c','cpu-backend/registry/kernel.goose'),('expert-cache.c','expert-cache/kernel.goose'),('cpu-control.c','cpu-backend/control/kernel.goose')]:
 run_cmd('generate-'+out,[str(a/'compiler/stage3-f849233b999c'),'-o',str(a/'generated'/out),str(a/'source'/src)])
if '--generate-only' in sys.argv:raise SystemExit(0)
for name,path in [('registry','cpu-backend/registry/unit.c'),('cache-goose','expert-cache/unit.c')]:run_cmd(name,inst(['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-Werror=implicit-function-declaration','-c',str(a/'source'/path),'-o',str(b/(name+'.o'))]))
recipes=json.loads((oldb/'commands.json').read_text())
for name in ['core','control','backend-cpp']:
 row=next(x for x in recipes if x['label']==name);run_cmd(name,[mapped(x) for x in row['argv']],row['cwd'])
# Relink only the changed CPU DSO; inherited base and other units retain their verified hashes.
row=next(x for x in recipes if x['label']=='link-cpu');argv=[mapped(x) for x in row['argv']]+[str(b/'registry.o')];run_cmd('link-cpu',argv,row['cwd'])
run_cmd('budget-services',inst(['/usr/bin/g++-13','-O3','-fPIC','-std=c++17','-c',str(a/'source/expert-cache/budget-services.cpp'),'-o',str(b/'budget-services.o')]))
for stem in ['libggml-cpu','libggml-base']:
 for suffix in ['.so','.so.0']:
  alias=run/(stem+suffix)
  if alias.exists() or alias.is_symlink():alias.unlink()
  alias.symlink_to(stem+'.so.0.27.0')
# Preserve the exact inherited llama overlay chain, replacing only the model-cache object.
commands=json.loads(Path('/home/end0/AI/LlamaServer/exp107-expert-overlap/build/commands.json').read_text())
cache=next(x for x in json.loads(Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/compile_commands.json').read_text()) if x['file'].endswith('llama-moe-cache.cpp'))
cmd=shlex.split(cache['command']);cmd[cmd.index('-o')+1]=str(b/'cache-cpp.o');cmd[-1]=str(a/'source/expert-cache/llama-moe-cache.cpp');run_cmd('cache-cpp',inst(cmd),cache['directory'])
cmd=commands['link-llama'];cmd=[x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('-o')+1]=str(run/'libllama-control.so.0.7.0');cmd=[str(b/'cache-cpp.o') if x=='src/CMakeFiles/llama.dir/llama-moe-cache.cpp.o' else x for x in cmd];cmd+=[str(b/'cache-goose.o'),str(b/'budget-services.o')];run_cmd('link-llama',inst(cmd),cache['directory'])
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:shutil.copy2(run/'libllama-control.so.0.7.0',run/name)
print(run,flush=True)
