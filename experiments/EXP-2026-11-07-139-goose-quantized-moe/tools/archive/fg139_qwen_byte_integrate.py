from pathlib import Path
import json,subprocess,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');old=r/'variants/qwen839a-moe-quantized';v=r/'variants/qwen839a-moe-quantized-byte';s=r/'source/moe-quantized-byte';(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True)
internal=(r/'source/moe-quantized/matrix-internal-unit.c').read_text().replace('generated/moe-matrix.c','generated/moe-matrix-byte.c');(s/'matrix-internal-unit.c').write_text(internal)
commands=[]
for label in ['math-cpu','math-base','base-link','cpu-link']:
 row=next(x for x in json.loads((old/'build/commands.json').read_text()) if x['label']==label);cmd=[]
 for arg in row['argv']:
  arg=arg.replace(str(r/'source/moe-quantized/matrix-internal-unit.c'),str(s/'matrix-internal-unit.c'))
  for name in ['math-cpu.o','math-base.o']:
   arg=arg.replace(str(old/'build'/name),str(v/'build'/name))
  for name in ['libggml-base.so.0.27.0','libggml-cpu.so.0.27.0']:
   arg=arg.replace(str(old/'run'/name),str(v/'run'/name))
  if arg.startswith('-Wl,-rpath,') or arg.startswith('-Wl,--dependency-file='):arg=arg.replace(str(old),str(v))
  cmd.append(arg)
 if label.startswith('math-'):cmd+=['--param=max-inline-insns-auto=2000','--param=max-inline-insns-single=4000','--param=inline-unit-growth=500']
 p=subprocess.run(cmd,cwd=row['cwd'],capture_output=True,text=True);(v/'build'/f'{label}.log').write_text(p.stdout+p.stderr);print(label,p.returncode,(p.stdout+p.stderr)[-1200:],flush=True);p.check_returncode();commands.append({'label':label,'cwd':row['cwd'],'argv':cmd})
for path in (old/'run').iterdir():
 if path.name.startswith(('libggml-cpu.so','libggml-base.so')):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for name in ['libggml-cpu','libggml-base']:
 for suffix in ['.so','.so.0']:
  p=v/'run'/(name+suffix)
  if not p.exists() and not p.is_symlink():p.symlink_to(name+'.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n');(v/'build/manifest.json').write_text(json.dumps({'status':'BUILT_GRAPH_AND_MODEL_PENDING','goose_sha256':hashlib.sha256((s/'matrix.goose').read_bytes()).hexdigest(),'emitted_c_sha256':hashlib.sha256((r/'generated/moe-matrix-byte.c').read_bytes()).hexdigest(),'cpu_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'base_sha256':hashlib.sha256((v/'run/libggml-base.so.0.27.0').read_bytes()).hexdigest(),'parent':str(old),'changed':'Only new Goose math objects; reuse exact native/fallback/activation/controller/allocator/CUDA objects','production_changed':False},indent=2)+'\n')
