from pathlib import Path
import re,json,subprocess,hashlib,os,struct
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');B=A/'build/cuda-unified';M=A/'source/cuda-unified/movement';p=M/'apex-move-device-primitives.cuh';s=p.read_text()
for name,typ in [('u16','uint16_t'),('i32','int32_t'),('i64','int64_t'),('f32','float')]:
 start=s.index('static __device__ __forceinline__ '+('float' if name=='f32' else 'int64_t')+' fg153_am_load_'+name+'(');end=s.index('\n}',start)+2
 s=s[:start]+f'''static __device__ __forceinline__ {'float' if name=='f32' else 'int64_t'} fg153_am_load_{name}(int64_t base, int64_t off) {{
    return *reinterpret_cast<const {typ} *>(am_ptr(base) + off);
}}'''+s[end:]
for name,typ,val in [('u16','uint16_t','int64_t'),('i32','int32_t','int64_t'),('f32','float','float')]:
 start=s.index('static __device__ __forceinline__ void fg153_am_store_'+name+'(');end=s.index('\n}',start)+2
 s=s[:start]+f'''static __device__ __forceinline__ void fg153_am_store_{name}(int64_t base, int64_t off, {val} v) {{
    *reinterpret_cast<{typ} *>(am_ptr(base) + off) = static_cast<{typ}>(v);
}}'''+s[end:]
p.write_text(s);records=json.loads((B/'commands.json').read_text());r=next(r for r in records if r['label']=='cuda-q4-offsets-build');args=r['argv'].copy();obj=B/'goose-move-typed-effects.o';args[args.index('-o')+1]=str(obj);q=subprocess.run(args,cwd=r['cwd'],capture_output=True,text=True,timeout=120);print('typed-effects-compile',q.returncode,flush=True)
if q.returncode:print(q.stderr[-5000:]);raise SystemExit(q.returncode)
records.append(dict(label='cuda-typed-effects-build',argv=args,cwd=r['cwd'],exit_code=q.returncode));r=next(r for r in records if r['label']=='cuda-q4-offsets-link');args=[str(obj) if x==str(B/'goose-move-q4-fixed.o') else x for x in r['argv']];out=A/'variants/cpu-backend/run'/('libggml-cuda-goose-typed-'+hashlib.sha256(obj.read_bytes()).hexdigest()[:12]+'.so.0.27.0');assert not out.exists();args[args.index('-o')+1]=str(out);q=subprocess.run(args,cwd=r['cwd'],capture_output=True,text=True);print('typed-effects-link',q.returncode,flush=True);assert q.returncode==0,q.stderr;records.append(dict(label='cuda-typed-effects-link',argv=args,cwd=r['cwd'],exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
for name in ['libggml-cuda.so','libggml-cuda.so.0','libggml-cuda.so.0.27.0']:
 p=B/'run'/name;p.unlink();p.symlink_to(out)
env=dict(os.environ,LD_LIBRARY_PATH=str(B/'run')+':'+str(A/'variants/cpu-backend/run'),CUDA_LAUNCH_BLOCKING='1');q=subprocess.run([str(B/'half-debug'),str(B/'goose-typed-stages.bin')],env=env,capture_output=True,text=True);print(q.returncode,q.stdout,flush=True);assert q.returncode==0,q.stderr
ar=[struct.unpack('<6144f',(B/n).read_bytes()) for n in ['native-stages.bin','goose-typed-stages.bin']];res=[]
for k,name in enumerate(['conv','norm','silu','mul','F16-roundtrip','Q4-roundtrip']):
 a=ar[0][k*1024:(k+1)*1024];b=ar[1][k*1024:(k+1)*1024];err=max(abs(x-y) for x,y in zip(a,b));res.append(dict(stage=name,max_abs_diff=err));print(name,err,flush=True)
j=json.loads((A/'results/cuda-numeric-linked.json').read_text());j.update(library=str(out),sha256=hashlib.sha256(out.read_bytes()).hexdigest(),stages=res,status='TYPED_RAW_EFFECTS_DEVICE_RECHECK',primitive_change='Aligned typed scalar access replaces byte-wise builtin memcpy lowering;all arithmetic remains Goose');(A/'results/cuda-numeric-linked.json').write_text(json.dumps(j,indent=2)+'\n')
