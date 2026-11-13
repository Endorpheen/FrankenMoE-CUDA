from pathlib import Path
import re,json,shlex,subprocess,os,shutil
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source';B=F/'runtime/exp173/build';R=F/'runtime/exp172/run';D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda');C=F/'runtime/compiler/stage3-f849233b999c';rows=[];env=dict(os.environ,LD_LIBRARY_PATH=str(R));env.pop('LD_PRELOAD',None)
assert __import__('hashlib').sha256((F/'runtime/compiler/stage3-f849233b999c').read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a','Pinned compiler changed'
assert all(__import__('hashlib').sha256(Path(p).read_bytes()).hexdigest()==h for p,h in json.loads((E/'external-header-pins.json').read_text()).items()),'Pinned external header changed; review first'

def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=600);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode,stdout=q.stdout,stderr=q.stderr[-6000:]));(B/'iter-commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-9000:]);raise SystemExit(q.returncode)
 return q
run('emit-iter',[C,'-o',S/'mma-iter-device-generated.c',S/'mma-iter-device.goose']);g=(S/'mma-iter-device-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];entries=[n for n in re.findall(r'\b(gg_function_\d+)\(',main)if n!='gg_function_0'];assert len(entries)==1;start=g.rfind('\n\n',0,re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g).start());types=g[start:g.index('static void gg_quote(')];funcs=['static void gg_fail(const char*){asm("trap;");}']
def body(src,m):
 at=m.end();depth=1
 while depth:depth+=(src[at]=='{')-(src[at]=='}');at+=1
 return src[m.start():at]
for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+|gg_view|gg_double_bits|gg_index|gg_div|gg_mod|gg_udiv|gg_umod)\([^;{}]*\)\s*\{',g):
 if m[1]!='gg_function_0':funcs.append(body(g,m))
qual=lambda s:re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg175mi_'+m[1],s)
layouts=[]
for m in re.finditer(r'struct (gg_type_\d+) \{\n([^}]+)\};',g):layouts.append((m[1],re.findall(r'\b(float|uint32_t) (f\d+);',m[2])))
alias='';fields_by_type={}
for name,tp,count in [('F4','float',4),('U4','uint32_t',4),('U2','uint32_t',2)]:
 t,fs=next((t,fs)for t,fs in layouts if len(fs)==count and all(x[0]==tp for x in fs));fields_by_type[name]=[f for _,f in fs];alias+='using fg175i_'+name+' = fg175mi_'+t+';\n'
(S/'mma-iter-types-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n'+qual('typedef struct{void*data;int64_t len;}gg_slice;\n'+types)+alias)
text=qual('\n'.join(f[:f.index('{')].rstrip()+';'for f in funcs)+'\n'+'\n'.join(funcs));text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in text
(S/'mma-iter-functions-generated.cuh').write_text('#pragma once\n'+text+'\n#define FG175I_ITER fg175mi_'+entries[0]+'\n')
prims=r'''#pragma once
#include "mma-iter-types-generated.cuh"
static __device__ __forceinline__ uint32_t fg176i_bits(float v){return __float_as_uint(v);}
static __device__ __forceinline__ float fg176i_frombits(uint32_t v){return __uint_as_float(v);}
static_assert(sizeof(fg175i_U2)==8&&sizeof(fg175i_U4)==16&&sizeof(fg175i_F4)==16,"Goose MMA POD widths");
static __device__ __forceinline__ void fg175l_cp16(uint64_t d,uint64_t s){unsigned p=__cvta_generic_to_shared((void*)(uintptr_t)d);asm volatile("cp.async.cg.shared.global.L2::64B [%0], [%1], 16;"::"r"(p),"l"(s):"memory");}
#include "loaders-functions-generated.cuh"
static __device__ __forceinline__ void fg175l_load_kv(uint64_t s,uint64_t d,int64_t stride,int64_t row,int32_t x,int32_t y){FG175L_KV(s,d,stride,row,x,y,1);}
static __device__ __forceinline__ void fg175l_load_mask(uint64_t s,uint64_t d,int64_t stride,int64_t row,int64_t query,int64_t nq,int32_t n,int32_t x,int32_t y){FG175L_MASK(s,d,stride,row,query,nq,n,x,y,1);}
static __device__ __forceinline__ uint32_t fg176i_ldu(uint64_t a){return*(const uint32_t*)(uintptr_t)a;}
static __device__ __forceinline__ void fg176i_stu(uint64_t a,uint32_t v){*(uint32_t*)(uintptr_t)a=v;}
static __device__ __forceinline__ fg175i_U4 fg176i_ldm4(uint64_t a){fg175i_U4 o;asm volatile("ldmatrix.sync.aligned.m8n8.x4.b16 {%0,%1,%2,%3},[%4];":"=r"(o.f0),"=r"(o.f1),"=r"(o.f2),"=r"(o.f3):"l"(a));return o;}
static __device__ __forceinline__ fg175i_U4 fg176i_ldm4t(uint64_t a){fg175i_U4 o;asm volatile("ldmatrix.sync.aligned.m8n8.x4.trans.b16 {%0,%1,%2,%3},[%4];":"=r"(o.f0),"=r"(o.f1),"=r"(o.f2),"=r"(o.f3):"l"(a));return o;}
static __device__ __forceinline__ fg175i_F4 fg176i_mma_f32(fg175i_F4 d,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t b0,uint32_t b1){asm("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 {%0,%1,%2,%3},{%4,%5,%6,%7},{%8,%9},{%0,%1,%2,%3};":"+f"(d.f0),"+f"(d.f1),"+f"(d.f2),"+f"(d.f3):"r"(a0),"r"(a1),"r"(a2),"r"(a3),"r"(b0),"r"(b1));return d;}
static __device__ __forceinline__ fg175i_U2 fg176i_mma_f16(fg175i_U2 d,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t b0,uint32_t b1){asm("mma.sync.aligned.m16n8k16.row.col.f16.f16.f16.f16 {%0,%1},{%2,%3,%4,%5},{%6,%7},{%0,%1};":"+r"(d.f0),"+r"(d.f1):"r"(a0),"r"(a1),"r"(a2),"r"(a3),"r"(b0),"r"(b1));return d;}
static __device__ __forceinline__ float fg176i_hlo(uint32_t w){return __half2float(__ushort_as_half((uint16_t)w));}
static __device__ __forceinline__ float fg176i_hhi(uint32_t w){return __half2float(__ushort_as_half(w>>16));}
static __device__ __forceinline__ uint32_t fg176i_h2(float a,float b){half2 z=__floats2half2_rn(a,b);uint32_t w;memcpy(&w,&z,4);return w;}
static __device__ __forceinline__ uint32_t fg176i_mulh2(uint32_t w,uint32_t s){half2 a,b;memcpy(&a,&w,4);memcpy(&b,&s,4);half2 z=__hmul2(a,b);uint32_t o;memcpy(&o,&z,4);return o;}
static __device__ __forceinline__ float fg176i_expf(float x){return expf(x);}
static __device__ __forceinline__ float fg176i_fmaf(float a,float b,float c){return __fmaf_rn(a,b,c);}
static __device__ __forceinline__ float fg176i_shflx(float x,int32_t d){return __shfl_xor_sync(0xffffffff,x,d,32);}
static __device__ __forceinline__ void fg176i_cpwait(){cp_async_wait_all();}
static __device__ __forceinline__ void fg176i_barrier(){__syncthreads();}
''';
for i,f in enumerate(fields_by_type['U4']):prims=prims.replace('o.f'+str(i), 'o.'+f)
prims=re.sub(r'(fg176i_mma_f32\([^\n]+)',lambda m:re.sub(r'd\.f([0-3])',lambda f:'d.'+fields_by_type['F4'][int(f[1])],m[0]),prims)
prims=re.sub(r'(fg176i_mma_f16\([^\n]+)',lambda m:re.sub(r'd\.f([0-1])',lambda f:'d.'+fields_by_type['U2'][int(f[1])],m[0]),prims)
(S/'mma-iter-primitives.cuh').write_text(prims)
fixture=r'''#include "fattn-mma-f16.cuh"
#include "mma-iter-primitives.cuh"
#include "mma-iter-functions-generated.cuh"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
template<int N,bool Original>__global__void iter_case(const half2*Q,const half2*K,const half2*V,const half*M,uint32_t*out,int nkv,int nq,uint3 nqdiv){constexpr int np=N==4?2:1;extern __shared__ half2 sm[];const int tx=threadIdx.x,ty=threadIdx.y,tid=ty*32+tx;for(int i=tid;i<N*8*132;i+=128)sm[i]=Q[(i/132)*128+i%132];__syncthreads();using QB=ggml_cuda_mma::tile<16,8,half2>;using VC=ggml_cuda_mma::tile<16,8,half2>;QB qregs[16];VC vregs[16];float mx[2]={-FLT_MAX/2,-FLT_MAX/2},rs[2]={};for(int q=0;q<16;q++)ggml_cuda_mma::load_ldmatrix(qregs[q],sm+(ty/np)*16*132+q*8,132);__syncthreads();flash_attn_ext_f16_load_mask<N,4,32,true,false,false>(M,(half*)(sm+8192),nkv,0,32,0,nqdiv,nullptr);flash_attn_ext_f16_load_tile<128,4,32,true,false,false>(K,sm,128,128,0,32,nullptr);for(int kb=0;kb<nkv/32;kb++){if constexpr(Original){if(kb+1<nkv/32)flash_attn_ext_f16_iter<256,256,N,8,4,false,false,false,false,false,false,false,QB,QB,ggml_cuda_mma::tile<16,16,float>,QB,QB,VC>(nullptr,K,V,M,nullptr,nullptr,nullptr,1,1,0,nqdiv,16,128,128,nkv,sm,sm,sm+4096,(half*)(sm+8192),qregs,vregs,mx,rs,0,kb,32);else flash_attn_ext_f16_iter<256,256,N,8,4,false,false,false,false,false,true,false,QB,QB,ggml_cuda_mma::tile<16,16,float>,QB,QB,VC>(nullptr,K,V,M,nullptr,nullptr,nullptr,1,1,0,nqdiv,16,128,128,nkv,sm,sm,sm+4096,(half*)(sm+8192),qregs,vregs,mx,rs,0,kb,32);}else FG175I_ITER((uint64_t)(uintptr_t)K,(uint64_t)(uintptr_t)V,(uint64_t)(uintptr_t)M,128,128,nkv,1,0,kb,nq,N,(uint64_t)(uintptr_t)qregs,(uint64_t)(uintptr_t)vregs,(uint64_t)(uintptr_t)mx,(uint64_t)(uintptr_t)rs,(uint64_t)(uintptr_t)sm,tx,ty,np,kb+1==nkv/32,1);}cp_async_wait_all();__syncthreads();for(int i=0;i<64;i++)out[tid*68+i]=((uint32_t*)vregs)[i];for(int i=0;i<2;i++){memcpy(out+tid*68+64+i,mx+i,4);memcpy(out+tid*68+66+i,rs+i,4);}}
static void ck(cudaError_t e){if(e!=cudaSuccess){fprintf(stderr,"CUDA %s\n",cudaGetErrorString(e));exit(2);}}
template<int N>void test(int nkv,int nq){std::vector<half2>q(N*8*132),k(nkv*128),v(k.size());std::vector<half>m(nq*nkv);for(size_t i=0;i<q.size();i++)q[i]=__floats2half2_rn(((int)(i%31)-15)/128.f,((int)(i%23)-11)/128.f);for(size_t i=0;i<k.size();i++){k[i]=__floats2half2_rn(((int)(i%17)-8)/64.f,((int)(i%19)-9)/64.f);v[i]=__floats2half2_rn(((int)(i%13)-6)/128.f,((int)(i%29)-14)/128.f);}for(size_t i=0;i<m.size();i++)m[i]=__float2half((i%nkv)<(size_t)(nkv-7)?0.f:-INFINITY);half2 *dq,*dk,*dv;half*dm;uint32_t*o[2];ck(cudaMalloc(&dq,q.size()*4));ck(cudaMalloc(&dk,k.size()*4));ck(cudaMalloc(&dv,v.size()*4));ck(cudaMalloc(&dm,m.size()*2));ck(cudaMemcpy(dq,q.data(),q.size()*4,cudaMemcpyHostToDevice));ck(cudaMemcpy(dk,k.data(),k.size()*4,cudaMemcpyHostToDevice));ck(cudaMemcpy(dv,v.data(),v.size()*4,cudaMemcpyHostToDevice));ck(cudaMemcpy(dm,m.data(),m.size()*2,cudaMemcpyHostToDevice));for(int side=0;side<2;side++){ck(cudaMalloc(o+side,128*68*4));ck(cudaMemset(o[side],0xA5,128*68*4));}iter_case<N,true><<<1,dim3(32,4),33792>>>(dq,dk,dv,dm,o[0],nkv,nq,init_fastdiv_values(nq));iter_case<N,false><<<1,dim3(32,4),33792>>>(dq,dk,dv,dm,o[1],nkv,nq,init_fastdiv_values(nq));ck(cudaGetLastError());ck(cudaDeviceSynchronize());std::vector<uint32_t>a(128*68),b(a.size());ck(cudaMemcpy(a.data(),o[0],a.size()*4,cudaMemcpyDeviceToHost));ck(cudaMemcpy(b.data(),o[1],b.size()*4,cudaMemcpyDeviceToHost));for(size_t i=0;i<a.size();i++)if(a[i]!=b[i]){fprintf(stderr,"FAIL MMAiter N%d nkv%d nq%d thread%zu slot%zu %08x %08x\n",N,nkv,nq,i/68,i%68,a[i],b[i]);exit(3);}ck(cudaFree(dq));ck(cudaFree(dk));ck(cudaFree(dv));ck(cudaFree(dm));ck(cudaFree(o[0]));ck(cudaFree(o[1]));printf("PASS MMAiter N%d nkv%d nq%d 8704words bitexact\n",N,nkv,nq);}
int main(){test<4>(64,3);test<8>(64,8);test<4>(96,4);test<8>(96,5);puts("PASS four MMA iteration state cases, no full-runtime/memory-safety/performance claim");}
'''.replace('__global__void','__global__ void');(S/'mma-iter-fixture.cu').write_text(fixture)
db=json.loads(Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/fattn-vec-instance-q4_0-q4_0.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(D));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'mma-iter-fixture.cu');cmd[cmd.index('-o')+1]=str(B/'mma-iter-fixture.o');run('iter-fixture-build',cmd,Path(r['directory']));run('iter-fixture-link',['/usr/bin/g++-13','-o',B/'mma-iter-fixture',B/'mma-iter-fixture.o','-L'+str(R),'-Wl,-rpath,'+str(R),'-lggml-cuda','-lggml-base','-lggml','-lcudart','-lcublas','-ldl','-lpthread']);q=run('iter-gpu-parity',[B/'mma-iter-fixture']);(E/'mma-iter-local-results.json').write_text(json.dumps(dict(status='PASS_SCOPED_STATE_ONLY',cases=4,stdout=q.stdout,runs=rows,production_changed=False,whole_runtime_and_safety_pending=True),indent=2)+'\n')
