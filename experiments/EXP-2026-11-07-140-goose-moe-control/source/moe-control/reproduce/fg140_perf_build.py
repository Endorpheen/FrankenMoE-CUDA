from pathlib import Path
import json,subprocess,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');old=r/'variants/moe-control-apex';v=r/'variants/moe-control-apex-perf';(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True);commands=[]
for row in json.loads((old/'build/commands.json').read_text()):
 cmd=[a.replace(str(old/'build'),str(v/'build')).replace(str(old/'run'),str(v/'run')).replace(str(r/'variants/moe-quantized/build/activation.o'),str(r/'variants/moe-activation-perf/build/activation.o')) for a in row['argv']]
 if row['label']=='core':cmd+=['-DFG140_TRACE=0']
 if row['label']=='controller':cmd+=['-DFG140_NO_WITNESS=1']
 p=subprocess.run(cmd,capture_output=True,text=True);(v/'build'/(row['label']+'.log')).write_text(p.stdout+p.stderr);print(row['label'],p.returncode,(p.stdout+p.stderr)[-1000:],flush=True);p.check_returncode();commands.append({'label':row['label'],'argv':cmd})
for path in (old/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for suffix in ['.so','.so.0']:
 p=v/'run'/('libggml-cpu'+suffix)
 if not p.exists() and not p.is_symlink():p.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n');(v/'build/manifest.json').write_text(json.dumps({'status':'BUILT_NOT_VALIDATED','change':'Omit new controller witnesses via no-op diagnostic primitive and core counters; use same previously compiled counter-free common activation object. Mathematical/generated Goose control statements unchanged.','cpu_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'production_changed':False},indent=2)+'\n')
