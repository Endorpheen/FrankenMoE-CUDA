from pathlib import Path
import json,subprocess,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
src=r/'variants/elementwise-vector-perf';v=r/'variants/moe-activation';(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True)
cmds=json.loads((src/'build/commands.json').read_text());link=next(c for c in cmds if '-shared' in c)
link=[x.replace(str(src/'run'),str(v/'run')).replace(str(r/'variants/cpu-moe/build/goose-q8.o'),str(r/'variants/moe-quantized/build/activation.o')) for x in link]
p=subprocess.run(link,capture_output=True,text=True);(v/'build/link.log').write_text(p.stdout+p.stderr);print('activation integration',p.returncode,(p.stdout+p.stderr)[-1500:],flush=True);p.check_returncode()
for path in (src/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for name in ['libggml-cpu.so','libggml-cpu.so.0']:
 p=v/'run'/name
 if not p.exists() and not p.is_symlink():p.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps([link],indent=2)+'\n')
(v/'build/manifest.json').write_text(json.dumps({'change':'Replace prior Q8_K-only Goose object with v8 common Q8_0/Q8_K quant module; original CPU native quantizers retained for GGML_GOOSE_MOE_QUANT=0','sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'trace_enabled':True,'production_changed':False},indent=2)+'\n')
