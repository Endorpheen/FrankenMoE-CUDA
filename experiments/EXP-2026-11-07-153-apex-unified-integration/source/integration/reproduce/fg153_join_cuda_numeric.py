from pathlib import Path
import subprocess,json,shlex,shutil,hashlib,os,time,concurrent.futures,threading
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');O=A/'helpers/omo5';S=A/'source/cuda-unified';B=A/'build/cuda-unified';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');R=A/'variants/cpu-backend/run';C=S/'ggml-cuda';C.mkdir(parents=True,exist_ok=True);B.mkdir(exist_ok=True)
if not (S/'prepared.json').exists():
 shutil.copytree(U/'ggml/src/ggml-cuda',C,dirs_exist_ok=True)
 for name in ['unary.cu','binbcast.cu','norm.cu','softmax.cu','rope.cu']:
  shutil.copyfile(O/'worker-9/gen/patchcheck/ggml/src/ggml-cuda'/name,C/name)
 for name in ['cpy.cu','getrows.cu','set-rows.cu','convert.cu']:shutil.copyfile(O/'worker-10/patched'/name,C/name)
 shutil.copyfile(O/'worker-11/patched/mmvq.cu',C/'mmvq.cu')
 for name in ['mmq.cu','quantize.cu']:shutil.copyfile(O/'worker-11/matrix-closure/patched'/name,C/name)
 for name in ['gated_delta_net.cu','ssm-conv.cu']:shutil.copyfile(O/'worker-13/patched'/name,C/name)
 for name in ['argsort.cu','top-k.cu','moe-weighted-reduction.cu']:shutil.copyfile(O/'worker-13/moe-routing/patched'/name,C/name)
 shutil.copyfile(O/'worker-12/active-fattn-vec.patched.cuh',C/'fattn-vec.cuh')
 (S/'prepared.json').write_text(json.dumps({'source':'Existing independently returned numeric packages 9..13','control':'worker14 repair pending;original lazy VMM backend preserved','created':time.time()},indent=2)+'\n')
common_flags=shlex.split(next(line for line in Path('/home/end0/AI/LlamaServer/exp101-kv-lazy/build/compile-cuda.sh').read_text().splitlines() if line.startswith("'/usr/bin/nvcc'")))
common_flags=common_flags[:common_flags.index('-x')]
# The candidate source tree takes precedence over all original CUDA headers.
extra=[C,O/'worker-9/patch',O/'worker-9/device',O/'worker-10',O/'worker-11',O/'worker-11/matrix-closure',O/'worker-12',O/'worker-13',O/'worker-13/patched',O/'worker-13/moe-routing',O/'worker-13/moe-routing/patched']
common_flags=[common_flags[0],*[arg for p in extra for arg in ['-I',str(p)]],*common_flags[1:]]
# Compile the current modified files and new generated units only; retain untouched objects.
changed=['unary.cu','binbcast.cu','norm.cu','softmax.cu','rope.cu','cpy.cu','getrows.cu','set-rows.cu','convert.cu','mmvq.cu','mmq.cu','quantize.cu','gated_delta_net.cu','ssm-conv.cu','argsort.cu','top-k.cu','moe-weighted-reduction.cu','template-instances/fattn-vec-instance-q4_0-q4_0.cu']
sources=[(C/name,B/(name.replace('/','__')+'.o')) for name in changed]
sources += [(p,B/('element-'+p.stem+'.o')) for p in sorted((O/'worker-9/gen/device').glob('*.cu'))]
sources += [(O/'worker-10/apex-move-device.cu',B/'goose-move.o'),(O/'worker-11/apex_matrix_device.cu',B/'goose-matrix.o'),(O/'worker-11/matrix-closure/closure_device.cu',B/'goose-matrix-closure.o'),(O/'worker-12/fattn-q4-kernel.cu',B/'goose-fa-q4.o'),(O/'worker-13/build/gdn.cu',B/'goose-gdn.o'),(O/'worker-13/build/conv.cu',B/'goose-conv.o'),(O/'worker-13/moe-routing/build/argsort.cu',B/'goose-moe-sort.o'),(O/'worker-13/moe-routing/build/reduce.cu',B/'goose-moe-reduce.o')]
records=[];lock=threading.Lock()
if (B/'commands.json').exists():records=json.loads((B/'commands.json').read_text())
def compile_one(pair):
 src,obj=pair;args=[*common_flags,'-x','cu','-c',str(src),'-o',str(obj)];key=hashlib.sha256((json.dumps(args)+hashlib.sha256(src.read_bytes()).hexdigest()).encode()).hexdigest();receipt=obj.with_suffix('.json')
 if obj.exists() and receipt.exists() and json.loads(receipt.read_text()).get('key')==key:return
 st=time.time();p=subprocess.run(args,cwd=U/'build-cuda',capture_output=True,text=True,timeout=1200)
 with lock:
  records.append(dict(label='cuda-'+obj.stem,argv=args,cwd=str(U/'build-cuda'),exit_code=p.returncode,seconds=time.time()-st));(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(obj.stem+'.stderr')).write_text(p.stderr);(B/(obj.stem+'.stdout')).write_text(p.stdout);print(obj.stem,p.returncode,round(time.time()-st,1),flush=True)
 if p.returncode:raise RuntimeError(obj.name+'\n'+p.stderr[-5000:])
 receipt.write_text(json.dumps(dict(key=key,source=str(src),sha256=hashlib.sha256(obj.read_bytes()).hexdigest()),indent=2)+'\n')
# Two bounded compile processes avoid repeating the earlier desktop memory incident.
errors=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
 for f in concurrent.futures.as_completed([pool.submit(compile_one,p) for p in sources]):
  try:f.result()
  except Exception as e:errors.append(str(e))
if errors:
 (B/'integration-errors.json').write_text(json.dumps(errors,indent=2)+'\n');print('\n'.join(errors));raise SystemExit(1)
line=next(line for line in Path('/home/end0/AI/LlamaServer/exp101-kv-lazy/build/link-candidate.sh').read_text().splitlines() if line.startswith("'/usr/bin/g++-13'"));args=shlex.split(line)
for i,arg in enumerate(args):
 if arg.endswith('.cu.o'):
  name=arg.split('ggml-cuda.dir/')[-1][:-2]
  if name in changed:args[i]=str(B/(name.replace('/','__')+'.o'))
for i,arg in enumerate(args):
 if arg.startswith('-Wl,--dependency-file='):args[i]='-Wl,--dependency-file='+str(B/'linked.d')
 if arg=='bin/libggml-base.so.0.27.0':args[i]=str(R/'libggml-base.so')
new=[obj for src,obj in sources if str(src).startswith(str(O))];args += [str(p) for p in new];digest=hashlib.sha256(b''.join(p.read_bytes() for src,p in sources)).hexdigest()[:12];out=R/('libggml-cuda-goose-numeric-'+digest+'.so.0.27.0');assert not out.exists();args[args.index('-o')+1]=str(out)
p=subprocess.run(args,cwd=U/'build-cuda',capture_output=True,text=True,timeout=180);(B/'link.stderr').write_text(p.stderr);records.append(dict(label='cuda-numeric-cohort-link',argv=args,cwd=str(U/'build-cuda'),exit_code=p.returncode));(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');print('LINK',p.returncode,flush=True)
if p.returncode:print(p.stderr[-6000:]);raise SystemExit(p.returncode)
(A/'results/cuda-numeric-linked.json').write_text(json.dumps(dict(status='LINKED_NOT_YET_GPU_QUALIFIED',library=str(out),sha256=hashlib.sha256(out.read_bytes()).hexdigest(),modified_native_tus=changed,new_goose_device_units=[str(p) for p in new],control='Original lazy-VMM backend retained until critical worker14 repair passes',model_loads=0,production_changed=False),indent=2)+'\n');print('CUDA numeric cohort linked;pending runtime gate.')
