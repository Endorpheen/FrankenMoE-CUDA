from pathlib import Path
import json,subprocess,hashlib,sys
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');profile=sys.argv[1];assert profile in ['apex','qwen'];old=r/'variants'/('tiled-all-'+profile);v=r/'variants'/('tiled-all-'+profile+'-perf');(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True);commands=[]
for path in (old/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for row in json.loads((old/'build/commands.json').read_text()):
 if row['label']=='native-tiled':continue
 cmd=[a.replace(str(old/'build/core.o'),str(v/'build/core.o')).replace(str(old/'build/goose-tiled.o'),str(v/'build/goose-tiled.o')).replace(str(old/'build/bridge.o'),str(v/'build/bridge.o')).replace(str(old/'run/libggml-cpu.so.0.27.0'),str(v/'run/libggml-cpu.so.0.27.0')) for a in row['argv']]
 cmd=[a.replace(str(old/'run'),str(v/'run')) if a.startswith('-Wl,-rpath,') else '-Wl,--dependency-file='+str(v/'build/cpu-link.d') if a.startswith('-Wl,--dependency-file=') else a for a in cmd]
 if row['label']=='core':cmd+=['-DFG140_TRACE=0']
 if row['label']=='goose-tiled':cmd=[x for x in cmd if x!='-DFG141_TRACE=1']+['-DFG141_TRACE=0']
 if row['label']=='bridge':cmd+=['-DFG141_BRIDGE_TRACE=0']
 if row['label']=='link':
  cmd=[x.replace(str(r/'variants'/('moe-control-'+profile)/'build/controller.o'),str(r/'variants'/('moe-control-'+profile+'-perf')/'build/controller.o')) for x in cmd]
  if profile=='apex':cmd=[x.replace(str(r/'variants/moe-quantized/build/activation.o'),str(r/'variants/moe-activation-perf/build/activation.o')) for x in cmd]
 p=subprocess.run(cmd,cwd=row['cwd'],capture_output=True,text=True);(v/'build'/(row['label']+'.log')).write_text(p.stdout+p.stderr);print(profile,row['label'],p.returncode,(p.stdout+p.stderr)[-1400:],flush=True);p.check_returncode();commands.append({'label':row['label'],'cwd':row['cwd'],'argv':cmd})
for suffix in ['.so','.so.0']:
 path=v/'run'/('libggml-cpu'+suffix)
 if not path.exists() and not path.is_symlink():path.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n');(v/'build/manifest.json').write_text(json.dumps({'status':'BUILT_VALIDATION_PENDING','cpu_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'change':'Disable new tiled and common-control diagnostics, preserve algorithms. Apex quant diagnostics disabled; inherited Qwen EXP139 diagnostics remain identical in both timing arms.','production_changed':False},indent=2)+'\n')
