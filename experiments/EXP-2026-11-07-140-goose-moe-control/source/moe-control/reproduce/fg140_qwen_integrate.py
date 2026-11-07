from pathlib import Path
import json,subprocess,shutil,shlex,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-control';old=r/'variants/qwen839a-moe-quantized-byte';v=r/'variants/moe-control-qwen';native=r/'variants/qwen839a-shared/build';(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True)
core=(r/'source/moe-quantized/ggml-cpu-qwen-quant.c').read_text();needle='static void ggml_compute_forward_mul_mat_id(';assert core.count(needle)==1;core=core.replace(needle,'static void fg140_previous_mul_mat_id(',1);needle='static void ggml_compute_forward(struct ggml_compute_params * params, struct ggml_tensor * tensor)';assert core.count(needle)==1;services=Path('/tmp/fg140_qwen_services.inc').read_text();shutil.copy2('/tmp/fg140_qwen_services.inc',s/'qwen-services.inc');core='#include "bridge.h"\nvoid fg139_quantize(int,const float*,void*,int64_t);\n'+core;core=core.replace(needle,services+'\n'+needle,1);(s/'ggml-cpu-qwen-control.c').write_text(core);commands=[]
def run(label,cmd,cwd=native):
 p=subprocess.run(cmd,cwd=cwd,capture_output=True,text=True);(v/'build'/f'{label}.log').write_text(p.stdout+p.stderr);print(label,p.returncode,(p.stdout+p.stderr)[-2000:],flush=True);p.check_returncode();commands.append({'label':label,'cwd':str(cwd),'argv':cmd})
oldcmds=json.loads((r/'variants/qwen839a-moe-quantized/build/commands.json').read_text());cmd=next(x['argv'] for x in oldcmds if x['label']=='cpu-core');cmd=[a.replace(str(r/'source/moe-quantized/ggml-cpu-qwen-quant.c'),str(s/'ggml-cpu-qwen-control.c')) for a in cmd]+['-I'+str(s)];cmd[cmd.index('-o')+1]=str(v/'build/core.o');run('core',cmd)
db=json.loads((native/'compile_commands.json').read_text());row=next(x for x in db if x['file'].endswith('/ggml-cpu-mul-mat-id-cold.c'));cmd=shlex.split(row['command']);cmd[cmd.index('-o')+1]=str(v/'build/cold-native.o');cmd+=['-Dggml_compute_forward_mul_mat_id_cold=fg140_previous_cold'];run('cold-native',cmd)
cmd=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-c',str(s/'unit.c'),'-o',str(v/'build/controller.o')];run('controller',cmd)
for p in (old/'run').iterdir():
 if p.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/p.name
 if not target.exists() and not target.is_symlink():target.symlink_to(p)
link=next(x['argv'] for x in json.loads((old/'build/commands.json').read_text()) if x['label']=='cpu-link');out=[]
for arg in link:
 if arg==str(old/'run/libggml-cpu.so.0.27.0'):arg=str(v/'run/libggml-cpu.so.0.27.0')
 elif arg==str(r/'variants/qwen839a-moe-quantized/build/cpu-core.o'):arg=str(v/'build/core.o')
 elif arg.endswith('ggml-cpu.dir/ggml-cpu/ggml-cpu-mul-mat-id-cold.c.o'):arg=str(v/'build/cold-native.o')
 elif arg.startswith('-Wl,-rpath,'):arg=arg.replace(str(old/'run'),str(v/'run'))
 elif arg.startswith('-Wl,--dependency-file='):arg='-Wl,--dependency-file='+str(v/'build/cpu-link.d')
 out.append(arg)
out.append(str(v/'build/controller.o'));run('link',out)
for suffix in ['.so','.so.0']:
 p=v/'run'/('libggml-cpu'+suffix)
 if not p.exists() and not p.is_symlink():p.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n');(v/'build/manifest.json').write_text(json.dumps({'status':'BUILT_NOT_GRAPH_OR_MODEL_TESTED','cpu_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'goose_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'preserved':'EXP139 five-format math/common quant, native cold fallback, bank domain, explicit fused window, ready hook, tiled, prior Goose base/allocator/CUDA/MTP','production_changed':False,'compiler_changed':False},indent=2)+'\n')
