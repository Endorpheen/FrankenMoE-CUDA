from pathlib import Path
import json,subprocess,shutil,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-control';v=r/'variants/moe-control-apex';(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True)
original=r/'source/cpu-moe-tiled/ggml-cpu-goose-tiled.c';core=original.read_text();needle='static void ggml_compute_forward_mul_mat_id(';assert core.count(needle)==1;core=core.replace(needle,'static void fg140_previous_mul_mat_id(',1);needle='static void ggml_compute_forward(struct ggml_compute_params * params, struct ggml_tensor * tensor)';assert core.count(needle)==1;services=Path('/tmp/fg140_apex_services.inc').read_text();shutil.copy2('/tmp/fg140_apex_services.inc',s/'apex-services.inc');core='#include "bridge.h"\nvoid fg139_quantize_q8_0(const float*,void*,int64_t);\n'+core;core=core.replace(needle,services+'\n'+needle,1);(s/'ggml-cpu-apex-control.c').write_text(core)
commands=[]
def run(label,cmd):
 p=subprocess.run(cmd,capture_output=True,text=True);(v/'build'/f'{label}.log').write_text(p.stdout+p.stderr);print(label,p.returncode,(p.stdout+p.stderr)[-1800:],flush=True);p.check_returncode();commands.append({'label':label,'argv':cmd})
cmd=next(x for x in json.loads((r/'variants/cpu-moe-tiled-perf/build/commands.json').read_text()) if '-c' in x and any('ggml-cpu-tiled.o' in a for a in x));cmd=[x.replace(str(original),str(s/'ggml-cpu-apex-control.c')) for x in cmd]+['-I'+str(s)];cmd[cmd.index('-o')+1]=str(v/'build/core.o');run('core',cmd)
cmd=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-c',str(s/'unit.c'),'-o',str(v/'build/controller.o')];run('controller',cmd)
link=json.loads((r/'variants/moe-activation/build/commands.json').read_text())[0];link=[x.replace(str(r/'variants/moe-activation/run'),str(v/'run')).replace(str(r/'variants/cpu-moe-tiled-perf/build/ggml-cpu-tiled.o'),str(v/'build/core.o')) for x in link]+[str(v/'build/controller.o')];run('link',link)
for p in (r/'variants/moe-activation/run').iterdir():
 if p.name.startswith('libggml-cpu.so'):continue
 q=v/'run'/p.name
 if not q.exists() and not q.is_symlink():q.symlink_to(p)
for suffix in ['.so','.so.0']:
 p=v/'run'/('libggml-cpu'+suffix)
 if not p.exists() and not p.is_symlink():p.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n');(v/'build/manifest.json').write_text(json.dumps({'status':'BUILT_NOT_GRAPH_OR_MODEL_TESTED','sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'source_parent_sha256':hashlib.sha256(original.read_bytes()).hexdigest(),'preserved':'All prior Goose Q4/Q6, tiled, common Q8 and vector elementwise objects; prior whole controller is disable fallback','production_changed':False},indent=2)+'\n')
