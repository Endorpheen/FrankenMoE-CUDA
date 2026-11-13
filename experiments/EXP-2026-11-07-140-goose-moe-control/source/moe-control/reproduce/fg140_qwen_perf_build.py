from pathlib import Path
import json,subprocess,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');old=r/'variants/moe-control-qwen';v=r/'variants/moe-control-qwen-perf';(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True);commands=[]
for path in (old/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for row in json.loads((old/'build/commands.json').read_text()):
 if row['label']=='cold-native':continue
 cmd=[a.replace(str(old/'build/core.o'),str(v/'build/core.o')).replace(str(old/'build/controller.o'),str(v/'build/controller.o')).replace(str(old/'run/libggml-cpu.so.0.27.0'),str(v/'run/libggml-cpu.so.0.27.0')) for a in row['argv']]
 cmd=[a.replace(str(old/'run'),str(v/'run')) if a.startswith('-Wl,-rpath,') else '-Wl,--dependency-file='+str(v/'build/cpu-link.d') if a.startswith('-Wl,--dependency-file=') else a for a in cmd]
 if row['label']=='core':cmd+=['-DFG140_TRACE=0']
 if row['label']=='controller':cmd+=['-DFG140_NO_WITNESS=1']
 p=subprocess.run(cmd,cwd=row['cwd'],capture_output=True,text=True);(v/'build'/(row['label']+'.log')).write_text(p.stdout+p.stderr);print(row['label'],p.returncode,(p.stdout+p.stderr)[-1300:],flush=True);p.check_returncode();commands.append({'label':row['label'],'cwd':row['cwd'],'argv':cmd})
for suffix in ['.so','.so.0']:
 path=v/'run'/('libggml-cpu'+suffix)
 if not path.exists() and not path.is_symlink():path.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n');(v/'build/manifest.json').write_text(json.dumps({'status':'BUILT_VALIDATION_PENDING','cpu_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'change':'Disable only new-controller diagnostic witnesses/counters; same prior EXP139 math/quant diagnostic objects are preserved identically in both model arms','production_changed':False},indent=2)+'\n')
