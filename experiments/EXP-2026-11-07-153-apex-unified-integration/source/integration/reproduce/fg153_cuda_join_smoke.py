from pathlib import Path
import json,subprocess,os,hashlib,math,struct
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=A/'source/cuda-unified';B=A/'build/cuda-unified';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');R=A/'variants/cpu-backend/run';T=B/'run';T.mkdir(exist_ok=True);j=json.loads((A/'results/cuda-numeric-linked.json').read_text());lib=Path(j['library'])
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(lib if f.name in ['libggml-cuda.so','libggml-cuda.so.0','libggml-cuda.so.0.27.0'] else f.resolve())
src=S/'joined-device-fixture.cpp';src.write_text('''#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cuda.h"
#include <vector>
#include <cstdio>
#include <cmath>
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 auto backend=ggml_backend_cuda_init(0);if(!backend)return 3;
 auto ctx=ggml_init({4*1024*1024,nullptr,true});if(!ctx)return 4;
 auto x=ggml_new_tensor_3d(ctx,GGML_TYPE_F32,11,128,1);
 auto w=ggml_new_tensor_2d(ctx,GGML_TYPE_F32,4,128);
 auto conv=ggml_ssm_conv(ctx,x,w);
 auto norm=ggml_rms_norm(ctx,conv,1e-5f);
 auto silu=ggml_silu(ctx,norm);
 auto mul=ggml_mul(ctx,norm,silu);
 auto half=ggml_cast(ctx,mul,GGML_TYPE_F16);
 auto full=ggml_cast(ctx,half,GGML_TYPE_F32);
 auto quant=ggml_cast(ctx,full,GGML_TYPE_Q4_0);
 auto result=ggml_cast(ctx,quant,GGML_TYPE_F32);
 auto graph=ggml_new_graph(ctx);ggml_build_forward_expand(graph,result);
 auto buf=ggml_backend_alloc_ctx_tensors(ctx,backend);if(!buf)return 5;
 std::vector<float> xv(11*128),wv(4*128);
 for(size_t i=0;i<xv.size();++i)xv[i]=float(int(i%31)-15)*0.03125f;
 for(size_t i=0;i<wv.size();++i)wv[i]=float(int(i%13)-6)*0.0625f;
 ggml_backend_tensor_set(x,xv.data(),0,xv.size()*4);ggml_backend_tensor_set(w,wv.data(),0,wv.size()*4);
 if(ggml_backend_graph_compute(backend,graph)!=GGML_STATUS_SUCCESS)return 6;
 ggml_backend_synchronize(backend);
 std::vector<float> out(ggml_nelements(result));ggml_backend_tensor_get(result,out.data(),0,out.size()*4);
 for(float v:out)if(!std::isfinite(v))return 7;
 FILE *f=fopen(argv[1],"wb");if(!f)return 8;fwrite(out.data(),4,out.size(),f);fclose(f);
 printf("PASS joined CUDA graph SSM-conv/RMS/SiLU/MUL/F16/Q4 movement elements=%zu\\n",out.size());
 ggml_backend_buffer_free(buf);ggml_free(ctx);ggml_backend_free(backend);return 0;
}
''')
records=[]
def run(label,args,env=None):
 args=list(map(str,args));p=subprocess.run(args,cwd=B,env=env,capture_output=True,text=True,timeout=120);records.append(dict(label=label,argv=args,cwd=str(B),exit_code=p.returncode));(B/'device-gate-commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,p.stdout[-1500:],flush=True)
 if p.returncode:print(p.stderr[-5000:]);raise SystemExit(p.returncode)
 return p
run('cuda-joined-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I'+str(U/'ggml/include'),src,'-L'+str(T),'-Wl,-rpath,'+str(T),'-lggml-cuda','-lggml','-lggml-base','-o',B/'joined-device-fixture'])
env=dict(os.environ,LD_LIBRARY_PATH=str(R),CUDA_LAUNCH_BLOCKING='1');run('cuda-joined-native',[B/'joined-device-fixture',B/'device-native.bin'],env)
env['LD_LIBRARY_PATH']=str(T)+':'+str(R)
run('cuda-joined-goose-trace',['/usr/local/bin/nsys','profile','--trace=cuda','--sample=none','--cpuctxsw=none','--force-overwrite=true','--output='+str(B/'joined-device-trace'),B/'joined-device-fixture',B/'device-goose.bin'],env)
a=struct.unpack('<1024f',(B/'device-native.bin').read_bytes());b=struct.unpack('<1024f',(B/'device-goose.bin').read_bytes());err=max(abs(x-y) for x,y in zip(a,b));print('MAX_ABS_DIFF',err,flush=True);assert err<=2e-3,err
p=run('cuda-joined-kernel-witness',['/usr/local/bin/nsys','stats','--report','cuda_gpu_kern_sum','--format','csv',B/'joined-device-trace.nsys-rep'])
assert 'fg153_goose_ssm_conv_kernel' in p.stdout,p.stdout[-2000:]
assert 'apex_kernel' in p.stdout or 'apex_' in p.stdout,p.stdout[-2000:]
j.update(status='PASS_REAL_CUDA_JOINED_GRAPH_NATIVE_DIFFERENTIAL',native_max_abs_diff=err,device_corpus='One joined SSM-conv/RMS/SiLU/MUL/F16/Q4 movement graph',goose_execution_witness='Actual Nsight CUDA kernel activity;not symbol presence alone',device_gate='build/cuda-unified/device-gate-commands.json',not_qualified='Full Apex inference / CUDA-control resources / performance remain pending');(A/'results/cuda-numeric-linked.json').write_text(json.dumps(j,indent=2)+'\n')
print('PASS actual CUDA joined graph;no model or benchmark.')
