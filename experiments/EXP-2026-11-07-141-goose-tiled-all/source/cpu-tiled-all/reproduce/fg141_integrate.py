from pathlib import Path
import json,subprocess,shlex,shutil,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-tiled-all';profile=__import__('sys').argv[1];assert profile in ['apex','qwen'];old=r/'variants'/('moe-control-'+profile);v=r/'variants'/('tiled-all-'+profile);(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True);commands=[];qsrc=Path('/home/end0/AI/LlamaServer/franken-qwen38-v31/source');apxsrc=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');native=r/'variants/qwen839a-shared/build' if profile=='qwen' else apxsrc/'build-cuda'
shutil.copy2('/tmp/fg141_native_bridge.cpp',s/'native-bridge.cpp');shutil.copy2(r/'source/moe-control/bridge.h',s/'controller-bridge.h')
core=(r/'source/moe-control'/f'ggml-cpu-{profile}-control.c').read_text();assert core.startswith('#include "bridge.h"');core=core.replace('#include "bridge.h"','#include "bridge.h"\n#include "controller-bridge.h"',1)
if profile=='apex':
 needle='uint8_t fg140_tiled(int64_t c,int64_t expert,int64_t rows){struct fg140_context*x=fg140_context_get(c);'
 assert needle in core;core=core.replace(needle,needle+'if(fg141_general_enabled()){const struct ggml_tensor*ids=x->dst->src[2];return ggml_compute_forward_mul_mat_id_tiled(x->params,x->dst,expert,rows,(const int32_t*)(x->rows+expert*ids->ne[0]*ids->ne[1]),x->scratch);}',1)
(s/f'ggml-cpu-{profile}-tiled.c').write_text(core)
def run(label,cmd,cwd=r):
 p=subprocess.run(cmd,cwd=cwd,capture_output=True,text=True);(v/'build'/f'{label}.log').write_text(p.stdout+p.stderr);print(profile,label,p.returncode,(p.stdout+p.stderr)[-1600:],flush=True);p.check_returncode();commands.append({'label':label,'cwd':str(cwd),'argv':cmd})
oldrows=json.loads((old/'build/commands.json').read_text());cmd=next(x['argv'] for x in oldrows if x['label']=='core');cmd=[a.replace(str(r/'source/moe-control'/f'ggml-cpu-{profile}-control.c'),str(s/f'ggml-cpu-{profile}-tiled.c')) for a in cmd]+['-I'+str(s)];cmd[cmd.index('-o')+1]=str(v/'build/core.o');run('core',cmd,native)
unit=json.loads((r/'variants/cpu-tiled-all/build/unit-command.json').read_text());unit=[a for a in unit if a not in ['-shared','-DFG141_KERNEL_ONLY=1']];unit[unit.index('-o')+1]=str(v/'build/goose-tiled.o');unit[unit.index(str(s/'unit.c')):unit.index(str(s/'unit.c'))]=['-c'];run('goose-tiled',unit)
db=json.loads((native/'compile_commands.json').read_text());row=next(x for x in db if x['file'].endswith('/ggml-cpu/tiled/tiled.cpp'));cpp=shlex.split(row['command'])
if profile=='apex':cpp=[a.replace(str(apxsrc),str(r/'source')) for a in cpp]
cpp[cpp.index('-o')+1]=str(v/'build/native-tiled.o');cpp+=['-Dggml_compute_forward_mul_mat_tiled=fg141_native_dense_tiled','-Dggml_compute_forward_mul_mat_id_tiled=fg141_native_moe_tiled'];run('native-tiled',cpp,native)
bridge=shlex.split(row['command']);bridge[bridge.index('-c')+1]=str(s/'native-bridge.cpp');bridge[bridge.index('-o')+1]=str(v/'build/bridge.o')
if profile=='apex':bridge=[a.replace(str(apxsrc),str(r/'source')) for a in bridge]
bridge+=['-I'+str(s)];run('bridge',bridge,native)
for path in (old/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
link=next(x['argv'] for x in oldrows if x['label']=='link');out=[]
for arg in link:
 if arg==str(old/'run/libggml-cpu.so.0.27.0'):arg=str(v/'run/libggml-cpu.so.0.27.0')
 elif arg==str(old/'build/core.o'):arg=str(v/'build/core.o')
 elif arg.endswith('ggml-cpu.dir/ggml-cpu/tiled/tiled.cpp.o'):arg=str(v/'build/native-tiled.o')
 elif arg.startswith('-Wl,-rpath,'):arg=arg.replace(str(old/'run'),str(v/'run'))
 elif arg.startswith('-Wl,--dependency-file='):arg='-Wl,--dependency-file='+str(v/'build/cpu-link.d')
 out.append(arg)
out += [str(v/'build/goose-tiled.o'),str(v/'build/bridge.o')];run('link',out,native)
for suffix in ['.so','.so.0']:
 path=v/'run'/('libggml-cpu'+suffix)
 if not path.exists() and not path.is_symlink():path.symlink_to('libggml-cpu.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n');(v/'build/manifest.json').write_text(json.dumps({'status':'BUILT_GGML_VALIDATION_PENDING','cpu_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'goose_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'preserved':'All EXP140 control and previous math/quant/base/CUDA objects; native tiled entrypoints renamed for disable/other-ISA fallback. Apex old Goose tiled retained when general disabled.','production_changed':False,'compiler_changed':False},indent=2)+'\n')
