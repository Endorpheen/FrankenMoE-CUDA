#include "host-ffi.h"
#include <atomic>
#include <cinttypes>
#include <cstdlib>
#include <type_traits>
struct fg167_packet{int64_t args[4];fg167_plan plan;};
static_assert(sizeof(uint64_t)==8&&sizeof(uint32_t)==4&&std::is_trivially_copyable<fg167_plan>::value,"Broadcast packet ABI");
static std::atomic<uint64_t>fg167_seen{0};static thread_local uint64_t fg167_thread_seen;
static bool fg167_enabled(){static const bool value=[](){const char*v=getenv("GGML_GOOSE_CUDA_BROADCAST");return!(v&&v[0]=='0'&&v[1]==0);}();return value;}
static void fg167_report();
static void fg167_run(int64_t op,ggml_backend_cuda_context&ctx,ggml_tensor*dst,int count=1){static const bool registered=[](){std::atexit(fg167_report);return true;}();(void)registered;uint64_t bit=UINT64_C(1)<<op;if(!(fg167_thread_seen&bit)){fg167_thread_seen|=bit;fg167_seen.fetch_or(bit,std::memory_order_relaxed);}fg167_packet p{};p.args[0]=op;p.args[1]=(int64_t)(uintptr_t)&ctx;p.args[2]=(int64_t)(uintptr_t)dst;p.args[3]=count;fg167_host_entry((int64_t)(uintptr_t)&p);}
int64_t fg167_arg(int64_t p,int64_t k){return((const fg167_packet*)(uintptr_t)p)->args[k];}
int64_t fg167_plan_ptr(int64_t p){return(int64_t)(uintptr_t)&((fg167_packet*)(uintptr_t)p)->plan;}
uint64_t fg167_hget(int64_t p,int64_t k,int64_t i){return fg167_project((const fg167_plan*)(uintptr_t)p,k,i);}
void fg167_hput(int64_t pp,int64_t k,int64_t i,uint64_t v){auto*p=(fg167_plan*)(uintptr_t)pp;if(k==0){p->ptrs[i]=v;return;}if(k>=1&&k<=3){p->dims[k-1][i]=v;return;}if(k>=4&&k<=6){p->strides[k-4][i]=v;return;}switch(k){case 7:p->magic[i/3][i%3]=v;return;case 8:p->grid[i]=v;return;case 9:p->threads[i]=v;return;case 10:p->kind_a=v;return;case 11:p->kind_b=v;return;case 12:p->kind_d=v;return;case 13:p->op=v;return;case 14:p->fuse=v;return;case 15:p->path=v;return;case 16:p->mix=v;return;default:GGML_ABORT("Invalid broadcast field write");}}
int64_t fg167_ti(int64_t p,int64_t k,int64_t i){const auto*t=(const ggml_tensor*)(uintptr_t)p;switch(k){case 0:return(int64_t)(uintptr_t)t->src[i];case 1:return(int64_t)(uintptr_t)t->data;case 2:return t->type;case 3:return t->ne[i];case 4:return t->nb[i];case 9:return(int64_t)ggml_nbytes(t);default:GGML_ABORT("Invalid broadcast tensor field");}}
uint8_t fg167_pred(int64_t op,int64_t a,int64_t b){const auto*x=(const ggml_tensor*)(uintptr_t)a;const auto*y=(const ggml_tensor*)(uintptr_t)b;switch(op){case 0:return ggml_is_contiguous(x);case 1:return ggml_is_permuted(x);case 3:return ggml_can_repeat(x,y);default:GGML_ABORT("Invalid broadcast predicate");}}
int64_t fg167_constant(int64_t k){switch(k){case 0:return GGML_TYPE_F32;case 1:return GGML_TYPE_F16;case 2:return GGML_TYPE_BF16;default:GGML_ABORT("Invalid broadcast constant");}}
void fg167_fail(int64_t k){GGML_ABORT("Goose CUDA broadcast validation failed:code=%" PRId64,k);}
static __device__ unsigned long long fg167_modes[3],fg167_types;
template<int Op,int Fuse,int Path,int KA,int KB,int KD>static __global__ void fg167_kernel(fg167_plan p){uint64_t x=(uint64_t)blockIdx.x*blockDim.x+threadIdx.x;uint64_t y=(uint64_t)blockIdx.y*blockDim.y+threadIdx.y;uint64_t z=(uint64_t)blockIdx.z*blockDim.z+threadIdx.z;uint64_t step=(uint64_t)blockDim.x*gridDim.x;if(x==0&&y==0&&z==0){atomicOr(&fg167_modes[Path],1ULL<<Op);atomicOr(&fg167_types,1ULL<<p.mix);}FG167_DEVICE_LANE((int64_t)(uintptr_t)&p,Op,Fuse,Path,KA,KB,KD,x,y,z,step,0);}
template<int Op,int Fuse,int Path,int KA,int KB,int KD>static void fg167_typed(fg167_packet*p){const auto&q=p->plan;auto&ctx=*(ggml_backend_cuda_context*)(uintptr_t)p->args[1];dim3 grid(q.grid[0],q.grid[1],q.grid[2]),threads(q.threads[0],q.threads[1],q.threads[2]);cudaStream_t stream=ctx.stream();if constexpr(Path==2){fg167_kernel<Op,Fuse,Path,KA,KB,KD><<<grid,threads,0,stream>>>(q);CUDA_CHECK(cudaGetLastError());}else{ggml_cuda_kernel_launch_params cfg(grid,threads,0,stream);ggml_cuda_kernel_launch(fg167_kernel<Op,Fuse,Path,KA,KB,KD>,cfg,q);}}
template<int Op,int Fuse,int Path>static void fg167_mix(fg167_packet*p){switch(p->plan.mix){
case 0:fg167_typed<Op,Fuse,Path,0,0,0>(p);break;
case 1:fg167_typed<Op,Fuse,Path,2,2,2>(p);break;
case 2:fg167_typed<Op,Fuse,Path,2,0,2>(p);break;
case 3:fg167_typed<Op,Fuse,Path,1,1,1>(p);break;
case 4:fg167_typed<Op,Fuse,Path,1,0,1>(p);break;
case 5:fg167_typed<Op,Fuse,Path,1,0,0>(p);break;
case 6:fg167_typed<Op,Fuse,Path,0,1,0>(p);break;
case 7:fg167_typed<Op,Fuse,Path,1,1,0>(p);break;
default:GGML_ABORT("Invalid broadcast type mix");}}
template<int Op,int Fuse>static void fg167_path(fg167_packet*p){if(p->plan.path==0)fg167_mix<Op,Fuse,0>(p);else fg167_mix<Op,Fuse,1>(p);}
template<int Op>static void fg167_fused(fg167_packet*p){switch(p->plan.fuse){
case 1:fg167_path<Op,1>(p);break;
case 2:fg167_path<Op,2>(p);break;
case 3:fg167_path<Op,3>(p);break;
case 4:fg167_path<Op,4>(p);break;
case 5:fg167_path<Op,5>(p);break;
case 6:fg167_path<Op,6>(p);break;
case 7:fg167_path<Op,7>(p);break;
case 8:fg167_path<Op,8>(p);break;
default:GGML_ABORT("Invalid broadcast fuse count");}}
void fg167_launch(int64_t pp){auto*p=(fg167_packet*)(uintptr_t)pp;if(p->plan.path==2){fg167_typed<5,1,2,0,0,0>(p);return;}switch(p->plan.op){case 0:fg167_path<0,1>(p);break;case 1:fg167_fused<1>(p);break;case 2:fg167_path<2,1>(p);break;case 3:fg167_fused<3>(p);break;case 4:fg167_path<4,1>(p);break;default:GGML_ABORT("Invalid broadcast op");}}
extern "C" __attribute__((visibility("default"))) uint64_t fg167_cuda_host_seen(){return fg167_seen.load();}
extern "C" __attribute__((visibility("default"))) int fg167_cuda_device_read(uint64_t*out){auto st=cudaMemcpyFromSymbol(out,fg167_modes,sizeof(fg167_modes));if(st!=cudaSuccess)return(int)st;return(int)cudaMemcpyFromSymbol(out+3,fg167_types,sizeof(fg167_types));}
static void fg167_report(){uint64_t seen=fg167_seen.load();if(!seen)return;uint64_t out[4]={};auto st=cudaMemcpyFromSymbol(out,fg167_modes,sizeof(fg167_modes));if(st==cudaSuccess)st=cudaMemcpyFromSymbol(out+3,fg167_types,sizeof(fg167_types));fprintf(stderr,"FG167_CUDA_BROADCAST host=0x%" PRIx64 ",cuda_status=%d,grid=0x%" PRIx64 ",unravel=0x%" PRIx64 ",repeat_back=0x%" PRIx64 ",types=0x%" PRIx64 "\n",seen,(int)st,out[0],out[1],out[2],out[3]);}
void ggml_cuda_op_repeat(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg167_enabled()){fg167_native_ggml_cuda_op_repeat(ctx,dst);return;}fg167_run(0,ctx,dst);}
void ggml_cuda_op_add(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg167_enabled()){fg167_native_ggml_cuda_op_add(ctx,dst);return;}fg167_run(1,ctx,dst);}
void ggml_cuda_op_sub(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg167_enabled()){fg167_native_ggml_cuda_op_sub(ctx,dst);return;}fg167_run(2,ctx,dst);}
void ggml_cuda_op_mul(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg167_enabled()){fg167_native_ggml_cuda_op_mul(ctx,dst);return;}fg167_run(3,ctx,dst);}
void ggml_cuda_op_div(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg167_enabled()){fg167_native_ggml_cuda_op_div(ctx,dst);return;}fg167_run(4,ctx,dst);}
void ggml_cuda_op_repeat_back(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg167_enabled()){fg167_native_ggml_cuda_op_repeat_back(ctx,dst);return;}fg167_run(5,ctx,dst);}
void ggml_cuda_op_fused_add(ggml_backend_cuda_context&ctx,ggml_tensor*dst,int count){if(!fg167_enabled()){fg167_native_ggml_cuda_op_fused_add(ctx,dst,count);return;}fg167_run(6,ctx,dst,count);}
void ggml_cuda_op_fused_mul(ggml_backend_cuda_context&ctx,ggml_tensor*dst,int count){if(!fg167_enabled()){fg167_native_ggml_cuda_op_fused_mul(ctx,dst,count);return;}fg167_run(7,ctx,dst,count);}
