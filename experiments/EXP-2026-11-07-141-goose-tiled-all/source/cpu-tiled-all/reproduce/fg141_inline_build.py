from pathlib import Path
import json,subprocess,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');old=r/'variants/tiled-all-apex-perf';v=r/'variants/tiled-all-apex-inline';(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True);rows=json.loads((old/'build/commands.json').read_text());flags=['--param=max-inline-insns-auto=2000','--param=max-inline-insns-single=4000','--param=inline-unit-growth=500'];commands=[]
(v/'build/hypothesis.json').write_text(json.dumps({'hypothesis':'Increase GCC inline budget to remove fixed-array aggregate ABI/helper cost in complete tiled module; unchanged emitted C and FP/check semantics.','flags':flags,'source_changed':False,'compiler_source_changed':False,'production_changed':False},indent=2)+'\n')
for label in ['goose-tiled','link']:
 row=next(x for x in rows if x['label']==label);cmd=[a.replace(str(old/'build/goose-tiled.o'),str(v/'build/goose-tiled.o')).replace(str(old/'run/libggml-cpu.so.0.27.0'),str(v/'run/libggml-cpu.so.0.27.0')) for a in row['argv']]
 cmd=[a.replace(str(old/'run'),str(v/'run')) if a.startswith('-Wl,-rpath,') else '-Wl,--dependency-file='+str(v/'build/cpu-link.d') if a.startswith('-Wl,--dependency-file=') else a for a in cmd]
 if label=='goose-tiled':cmd+=flags
 p=subprocess.run(cmd,cwd=row['cwd'],capture_output=True,text=True);(v/'build'/(label+'.log')).write_text(p.stdout+p.stderr);print(label,p.returncode,(p.stdout+p.stderr)[-1400:],flush=True);p.check_returncode();commands.append({'label':label,'cwd':row['cwd'],'argv':cmd})
for path in (old/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for suffix in ['.so','.so.0']:
 path=v/'run'/('libggml-cpu'+suffix)
 if not path.exists() and not path.is_symlink():path.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n');(v/'build/manifest.json').write_text(json.dumps({'status':'BUILT_VALIDATION_PENDING','cpu_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'emitted_c_sha256':hashlib.sha256((r/'generated/cpu-tiled-all.c').read_bytes()).hexdigest(),'change':'Only host GCC inline limits for Goose tiled object; all other objects and source unchanged'},indent=2)+'\n')
