from pathlib import Path
import re,json,shlex,subprocess,os,hashlib
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source';B=F/'runtime/exp173/build';R=F/'runtime/exp172/run';D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda');C=F/'runtime/compiler/stage3-f849233b999c';rows=[];env=dict(os.environ,LD_LIBRARY_PATH=str(R));env.pop('LD_PRELOAD',None)
assert __import__('hashlib').sha256((F/'runtime/compiler/stage3-f849233b999c').read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a','Pinned compiler changed'
assert all(__import__('hashlib').sha256(Path(p).read_bytes()).hexdigest()==h for p,h in json.loads((E/'external-header-pins.json').read_text()).items()),'Pinned external header changed; review first'

def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=600);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode,stdout=q.stdout,stderr=q.stderr[-7000:]));(B/'staging-commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-10000:]);raise SystemExit(q.returncode)
 return q
run('emit-staging',[C,'-o',S/'staging-device-generated.c',S/'staging-device.goose']);g=(S/'staging-device-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];entries=[n for n in re.findall(r'\b(gg_function_\d+)\(',main)if n!='gg_function_0'];assert len(entries)==2
start=g.rfind('\n\n',0,re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g).start());types=g[start:g.index('static void gg_quote(')];funcs=['static void gg_fail(const char*){asm("trap;");}']
def body(src,m):
 at=m.end();depth=1
 while depth:depth+=(src[at]=='{')-(src[at]=='}');at+=1
 return src[m.start():at]
for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+|gg_view|gg_double_bits|gg_index|gg_div|gg_mod|gg_udiv|gg_umod)\([^;{}]*\)\s*\{',g):
 if m[1]!='gg_function_0':funcs.append(body(g,m))
text='typedef struct{void*data;int64_t len;}gg_slice;\n'+types+'\n'+'\n'.join(f[:f.index('{')].rstrip()+';'for f in funcs)+'\n'+'\n'.join(funcs);text=re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg175qd_'+m[1],text);text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in text
(S/'staging-functions-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n'+text+'\n#define FG175Q_STAGE fg175qd_'+entries[0]+'\n#define FG175Q_CONTIG fg175qd_'+entries[1]+'\n')
native=(D/'convert.cu').read_text();m=re.search(r'template <int qk, int qr, dequantize_kernel_t dequantize_kernel, typename dst_t>\nstatic __global__ void dequantize_block\([^{}]+\{',native);assert m;a=body(native,m);m=re.search(r'template<typename dst_t>\nstatic __global__ void dequantize_block_q4_0\([^{}]+\{',native);assert m;b=body(native,m);(S/'staging-native-reference.cuh').write_text('#pragma once\n#include "dequantize.cuh"\n'+a+'\n'+b+'\n')
source=r'''#include "staging-native-reference.cuh"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static __device__ __forceinline__ uint32_t fg175q_ld_u8(uint64_t p,uint64_t b){return*((const uint8_t*)(uintptr_t)p+b);}
static __device__ __forceinline__ uint32_t fg175q_ld_u16(uint64_t p,uint64_t b){return*(const uint16_t*)((const char*)(uintptr_t)p+b);}
static __device__ __forceinline__ float fg175q_h2f(uint32_t b){return __half2float(__ushort_as_half((uint16_t)b));}
static __device__ __forceinline__ void fg175q_st_f16(uint64_t p,uint64_t b,float v){*(half*)((char*)(uintptr_t)p+b)=__float2half_rn(v);}
#include "staging-functions-generated.cuh"
__global__ void fg175q_generic(const void*x,half*y,int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,uint64_t nb1,uint64_t nb2,uint64_t nb3){FG175Q_STAGE((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)y,ne0,ne1,ne2,ne3,nb1,nb2,nb3,blockIdx.x,blockIdx.y,blockIdx.z,gridDim.x,gridDim.y,gridDim.z,threadIdx.x,blockDim.x,1);}
__global__ void fg175q_contig(const void*x,half*y,int64_t blocks){FG175Q_CONTIG((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)y,blocks,blockIdx.x,threadIdx.x,1);}
static void ck(cudaError_t e){if(e!=cudaSuccess){fprintf(stderr,"CUDA %s\n",cudaGetErrorString(e));exit(2);}}
static void req(bool b,const char*w){if(!b){fprintf(stderr,"FAIL %s\n",w);exit(3);}}
static size_t checks=0;
static void test(bool contig,int n1,int n2,int n3,int pad,int blocks=0){const int ne0=256;const size_t nb1=144+pad,nb2=nb1*n1+pad,nb3=nb2*n2+pad,guard=64;const size_t xn=contig?blocks*18:(n3-1)*nb3+(n2-1)*nb2+(n1-1)*nb1+144;const size_t yn=contig?blocks*32*2:ne0*n1*n2*n3*2;std::vector<unsigned char>x(xn+2*guard,0xA5),ya(yn+2*guard,0xA5),yb=ya;const uint16_t scales[]={0x0000,0x8000,0x2800,0xa800,0x3001,0xb345,0x0400,0x0001};auto fill=[&](size_t at,int b){uint16_t d=scales[b%8];memcpy(x.data()+guard+at,&d,2);for(int j=0;j<16;j++)x[guard+at+2+j]=(unsigned char)(((j+b)%16)|(((3*j+b)%16)<<4));};if(contig){for(int b=0;b<blocks;b++)fill(b*18,b);}else for(int i3=0;i3<n3;i3++)for(int i2=0;i2<n2;i2++)for(int i1=0;i1<n1;i1++)for(int b=0;b<8;b++)fill(i3*nb3+i2*nb2+i1*nb1+b*18,b+i1+i2+i3);unsigned char *dx,*a,*b;ck(cudaMalloc(&dx,x.size()));ck(cudaMalloc(&a,ya.size()));ck(cudaMalloc(&b,yb.size()));ck(cudaMemcpy(dx,x.data(),x.size(),cudaMemcpyHostToDevice));ck(cudaMemcpy(a,ya.data(),ya.size(),cudaMemcpyHostToDevice));ck(cudaMemcpy(b,yb.data(),yb.size(),cudaMemcpyHostToDevice));if(contig){dequantize_block_q4_0<half><<<(blocks+7)/8,32>>>(dx+guard,(half*)(a+guard),blocks);fg175q_contig<<<(blocks+7)/8,32>>>(dx+guard,(half*)(b+guard),blocks);}else{dim3 grid(1,n1>2?2:n1,n2*n3>3?3:n2*n3);dequantize_block<32,2,dequantize_q4_0,half><<<grid,256>>>(dx+guard,(half*)(a+guard),ne0,n1,n2*n3,init_fastdiv_values(n2),nb1/18,nb2/18,nb3/18);fg175q_generic<<<grid,256>>>(dx+guard,(half*)(b+guard),ne0,n1,n2,n3,nb1,nb2,nb3);}ck(cudaGetLastError());ck(cudaDeviceSynchronize());ck(cudaMemcpy(ya.data(),a,ya.size(),cudaMemcpyDeviceToHost));ck(cudaMemcpy(yb.data(),b,yb.size(),cudaMemcpyDeviceToHost));for(size_t i=0;i<ya.size();i++){checks++;if(ya[i]!=yb[i]){fprintf(stderr,"FAIL stage contig=%d n1=%d n2=%d n3=%d pad=%d byte=%zu expected=%02x actual=%02x\n",contig,n1,n2,n3,pad,i,ya[i],yb[i]);exit(3);}}for(size_t i=0;i<guard;i++)req(yb[i]==0xA5&&yb[yb.size()-1-i]==0xA5,"stage output guards");std::vector<unsigned char>xr(x.size());ck(cudaMemcpy(xr.data(),dx,xr.size(),cudaMemcpyDeviceToHost));req(xr==x,"stage readonly input and guards");ck(cudaFree(dx));ck(cudaFree(a));ck(cudaFree(b));printf("PASS Q4stage contig%d n1%d n2%d n3%d pad%d blocks%d\n",contig,n1,n2,n3,pad,blocks);}
int main(){test(false,1,2,1,0);test(false,5,2,2,18);test(false,3,2,2,36);test(true,1,1,1,0,8);test(true,1,1,1,0,7);test(true,1,1,1,0,17);printf("PASS Q4-F16 cases6 bytechecks%zu native-bitwise+guards+immutable-input;no timings\n",checks);}
'''
(S/'staging-fixture.cu').write_text(source);db=json.loads(Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/fattn-vec-instance-q4_0-q4_0.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(D));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'staging-fixture.cu');cmd[cmd.index('-o')+1]=str(B/'staging-fixture.o');run('staging-fixture-build',cmd,Path(r['directory']));run('staging-fixture-link',['/usr/bin/g++-13','-o',B/'staging-fixture',B/'staging-fixture.o','-L'+str(R),'-Wl,-rpath,'+str(R),'-lggml-cuda','-lggml-base','-lggml','-lcudart','-lcublas','-ldl','-lpthread']);q=run('staging-gpu-parity',[B/'staging-fixture']);(E/'staging-local-results.json').write_text(json.dumps(dict(status='PASS_COMPONENT_ONLY_NOT_RUNTIME_ACCEPTED',cases=6,stdout=q.stdout,runs=rows,production_changed=False),indent=2)+'\n')
