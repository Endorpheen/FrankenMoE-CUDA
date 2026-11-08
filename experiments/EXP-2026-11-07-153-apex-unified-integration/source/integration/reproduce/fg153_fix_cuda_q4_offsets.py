from pathlib import Path
import json,subprocess,shutil,hashlib,os
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');O=A/'helpers/omo5';B=A/'build/cuda-unified';S=A/'source/cuda-unified';M=S/'movement';M.mkdir(exist_ok=True);(M/'generated').mkdir(exist_ok=True)
for p in (O/'worker-10').iterdir():
 if p.is_file() and p.suffix in ['.c','.h','.cu','.cuh','.goose','.py']:shutil.copyfile(p,M/p.name)
s=(M/'apex-move.goose').read_text();s=s.replace('let so=(i00/QK4_0)*param(p,P_NB00)+','let so=(if st==T_Q4_0 {i00/QK4_0}else{i00})*param(p,P_NB00)+',1).replace('let do=(j00/QK4_0)*param(p,P_NB10)+','let do=(if dt==T_Q4_0 {j00/QK4_0}else{j00})*param(p,P_NB10)+',1).replace('dequant_q4_to_float_into(param(p,P_SRC),0,j,param(p,P_DST),do,QK4_0);','dequant_q4_to_float_into(param(p,P_SRC)+so,0,j,param(p,P_DST),do,QK4_0);');(M/'apex-move.goose').write_text(s)
records=json.loads((B/'commands.json').read_text())
def run(label,args,cwd=B):
 args=list(map(str,args));p=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=150);records.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=p.returncode));(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print(p.stderr[-5000:]);raise SystemExit(p.returncode)
run('cuda-q4-offsets-emit',[A/'compiler/stage3-f849233b999c','-o',M/'generated/apex-move.c',M/'apex-move.goose'])
run('cuda-q4-offsets-qualify',['python3',M/'apex-move-adapt.py','--generated',M/'generated/apex-move.c','--cuh',M/'generated/apex-move-device-functions.cuh','--ids',M/'generated/apex-move-entry-ids.h','--report',M/'generated/emit-report.json','--consts',M/'generated/apex-move-const-init.h'])
row=next(r for r in records if r['label']=='cuda-goose-move');args=row['argv'].copy();args[args.index('-c')+1]=str(M/'apex-move-device.cu');obj=B/'goose-move-q4-fixed.o';args[args.index('-o')+1]=str(obj);run('cuda-q4-offsets-build',args,Path(row['cwd']))
row=next(r for r in records if r['label']=='cuda-numeric-complete-link');args=[str(obj) if x==str(B/'goose-move.o') else x for x in row['argv']];out=A/'variants/cpu-backend/run'/('libggml-cuda-goose-q4-fix-'+hashlib.sha256(obj.read_bytes()).hexdigest()[:12]+'.so.0.27.0');assert not out.exists();args[args.index('-o')+1]=str(out);run('cuda-q4-offsets-link',args,Path(row['cwd']))
for name in ['libggml-cuda.so','libggml-cuda.so.0','libggml-cuda.so.0.27.0']:
 q=B/'run'/name;q.unlink();q.symlink_to(out)
j=json.loads((A/'results/cuda-numeric-linked.json').read_text());j.update(status='Q4_OFFSETS_FIXED_PENDING_DEVICE_RECHECK',library=str(out),sha256=hashlib.sha256(out.read_bytes()).hexdigest(),critical_failure='Initial real GPU joined graph exposed F16/Q4 movement divergence;not qualified',q4_fix='Distinct scalar/block byte strides and advanced source block address');(A/'results/cuda-numeric-linked.json').write_text(json.dumps(j,indent=2)+'\n')
for name,d in [('goose',B/'run')]:
 env=dict(os.environ,LD_LIBRARY_PATH=str(d)+':'+str(A/'variants/cpu-backend/run'),CUDA_LAUNCH_BLOCKING='1');q=subprocess.run([str(B/'joined-device-fixture-stages'),str(B/(name+'-fixed-stages.bin'))],env=env,capture_output=True,text=True);print(name,q.returncode,q.stdout,flush=True);assert q.returncode==0
import struct
ar=[struct.unpack('<6144f',(B/n).read_bytes()) for n in ['native-stages.bin','goose-fixed-stages.bin']]
for k,name in enumerate(['conv','norm','silu','mul','F16-roundtrip','Q4-roundtrip']):
 a=ar[0][k*1024:(k+1)*1024];b=ar[1][k*1024:(k+1)*1024];err=max(abs(x-y) for x,y in zip(a,b));print(name,err,flush=True)
