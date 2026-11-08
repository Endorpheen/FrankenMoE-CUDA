#include "host-ffi.h"
#include "plan.h"
#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <type_traits>
struct fg166_packet{int64_t args[4];fg166_plan plan;};
static_assert(sizeof(int64_t)==8&&sizeof(float)==4&&std::is_trivially_copyable<fg166_plan>::value,"Host/device packet ABI");
static std::atomic<uint64_t>fg166_seen{0};static thread_local uint64_t fg166_thread_seen;
static bool fg166_enabled(){static const bool value=[](){const char*v=getenv("GGML_GOOSE_CUDA_ACTIVATIONS");return!(v&&v[0]=='0'&&v[1]==0);}();return value;}
static void fg166_report();
static void fg166_run(int64_t code,ggml_backend_cuda_context&ctx,ggml_tensor*a,ggml_tensor*b=nullptr){static const bool registered=[](){std::atexit(fg166_report);return true;}();(void)registered;uint64_t bit=UINT64_C(1)<<code;if(!(fg166_thread_seen&bit)){fg166_thread_seen|=bit;fg166_seen.fetch_or(bit,std::memory_order_relaxed);}fg166_packet p{};p.args[0]=code;p.args[1]=(int64_t)(uintptr_t)&ctx;p.args[2]=(int64_t)(uintptr_t)a;p.args[3]=(int64_t)(uintptr_t)b;fg166_host_entry((int64_t)(uintptr_t)&p);}
int64_t fg166_arg(int64_t p,int64_t k){return((const fg166_packet*)(uintptr_t)p)->args[k];}
int64_t fg166_info(int64_t op,int64_t key){static const int64_t data[][3]={
{0,0,0},
{0,1,0},
{0,2,0},
{0,3,0},
{0,4,0},
{0,5,0},
{0,6,0},
{0,7,0},
{0,8,0},
{0,9,0},
{0,10,0},
{0,11,0},
{0,12,0},
{0,13,0},
{0,14,0},
{0,15,0},
{0,16,0},
{0,17,0},
{0,18,0},
{0,19,0},
{0,20,0},
{0,21,0},
{0,22,0},
{0,23,0},
{0,24,0},
{0,25,0},
{2,9,1},
{2,4,1},
{2,7,1},
{2,5,1},
{2,6,1},
{3,0,1},
{4,0,1},
{5,0,2},
{6,0,3},
{9,0,4},
{7,0,5},
{8,0,6},
{2,0,7},
{0,26,8},
};GGML_ASSERT(op>=0&&op<(int64_t)(sizeof(data)/sizeof(data[0]))&&key>=0&&key<3);return data[op][key];}
int64_t fg166_ti(int64_t p,int64_t key,int64_t i){const auto*t=(const ggml_tensor*)(uintptr_t)p;switch(key){case 0:return(int64_t)(uintptr_t)t->src[i];case 1:return(int64_t)(uintptr_t)t->data;case 2:return t->type;case 3:return t->ne[i];case 4:return t->nb[i];case 5:return ggml_nelements(t);case 6:return ggml_nrows(t);case 7:return ggml_get_unary_op(t);case 8:{int32_t v;memcpy(&v,(const char*)t->op_params+i*4,4);return v;}default:GGML_ABORT("Invalid CUDA activation field");}}
float fg166_tf(int64_t p,int64_t i){float v;memcpy(&v,(const char*)((const ggml_tensor*)(uintptr_t)p)->op_params+i*4,4);return v;}
int64_t fg166_pred(int64_t k,int64_t a,int64_t b){const auto*x=(const ggml_tensor*)(uintptr_t)a;const auto*y=(const ggml_tensor*)(uintptr_t)b;switch(k){case 0:return ggml_is_contiguous(x);case 1:return ggml_is_contiguous_1(x);case 2:return ggml_is_contiguous_rows(x);case 3:return ggml_are_same_shape(x,y);default:GGML_ABORT("Invalid CUDA activation predicate");}}
void fg166_hi(int64_t p,int64_t k,int64_t v){((fg166_packet*)(uintptr_t)p)->plan.iv[k]=v;}
void fg166_hf(int64_t p,int64_t k,float v){((fg166_packet*)(uintptr_t)p)->plan.fv[k]=v;}
int64_t fg166_constant(int64_t k){switch(k){case 0:return GGML_TYPE_F32;case 1:return GGML_TYPE_F16;case 2:return GGML_TYPE_BF16;case 3:return CUDA_NEG_BLOCK_SIZE;case 4:return CUDA_GLU_BLOCK_SIZE;case 5:return CUDA_XIELU_BLOCK_SIZE;case 6:return CUDA_SILU_BACK_BLOCK_SIZE;case 7:return CUDA_RELU_BLOCK_SIZE;case 8:return GGML_UNARY_OP_SILU;case 9:return GGML_UNARY_OP_SIGMOID;case 10:return GGML_UNARY_OP_SOFTPLUS;case 11:return CUDA_SCALE_BLOCK_SIZE;case 12:return CUDA_CLAMP_BLOCK_SIZE;default:GGML_ABORT("Invalid CUDA activation constant");}}
float fg166_half_round(float v){return __half2float(__float2half_rn(v));}
void fg166_fail(int64_t k){GGML_ABORT("Goose CUDA activation validation failed:code=%" PRId64,k);}
static __device__ unsigned long long fg166_device_modes[10];
template<int Mode,int Op,int Kind>static __global__ void fg166_kernel(fg166_plan p){int64_t tid=(int64_t)blockIdx.x*blockDim.x+threadIdx.x;int64_t stride=(int64_t)blockDim.x*gridDim.x;if(tid==0)atomicOr(&fg166_device_modes[Mode],1ULL<<Op);FG166_DEVICE_LANE((int64_t)(uintptr_t)&p,Mode,Op,Kind,tid,stride,0);}
template<int Mode,int Op,int Kind>static void fg166_launch_typed(fg166_packet*p){const auto&plan=p->plan;auto&ctx=*(ggml_backend_cuda_context*)(uintptr_t)p->args[1];cudaStream_t stream=ctx.stream();if constexpr(Mode<=2||Mode==7){ggml_cuda_kernel_launch_params cfg((dim3)plan.iv[18],(dim3)plan.iv[19],0,stream);ggml_cuda_kernel_launch(fg166_kernel<Mode,Op,Kind>,cfg,plan);}else{fg166_kernel<Mode,Op,Kind><<<(unsigned int)plan.iv[18],(unsigned int)plan.iv[19],0,stream>>>(plan);CUDA_CHECK(cudaGetLastError());}}
template<int Mode,int Op>static void fg166_launch_kind(fg166_packet*p){switch(p->plan.iv[22]){case 0:fg166_launch_typed<Mode,Op,0>(p);break;case 1:fg166_launch_typed<Mode,Op,1>(p);break;case 2:fg166_launch_typed<Mode,Op,2>(p);break;default:GGML_ABORT("Invalid CUDA activation dtype");}}
void fg166_launch(int64_t pp){auto*p=(fg166_packet*)(uintptr_t)pp;switch(p->plan.iv[20]){
case 0:switch(p->plan.iv[21]){
case 0:fg166_launch_kind<0,0>(p);break;
case 1:fg166_launch_kind<0,1>(p);break;
case 2:fg166_launch_kind<0,2>(p);break;
case 3:fg166_launch_kind<0,3>(p);break;
case 4:fg166_launch_kind<0,4>(p);break;
case 5:fg166_launch_kind<0,5>(p);break;
case 6:fg166_launch_kind<0,6>(p);break;
case 7:fg166_launch_kind<0,7>(p);break;
case 8:fg166_launch_kind<0,8>(p);break;
case 9:fg166_launch_kind<0,9>(p);break;
case 10:fg166_launch_kind<0,10>(p);break;
case 11:fg166_launch_kind<0,11>(p);break;
case 12:fg166_launch_kind<0,12>(p);break;
case 13:fg166_launch_kind<0,13>(p);break;
case 14:fg166_launch_kind<0,14>(p);break;
case 15:fg166_launch_kind<0,15>(p);break;
case 16:fg166_launch_kind<0,16>(p);break;
case 17:fg166_launch_kind<0,17>(p);break;
case 18:fg166_launch_kind<0,18>(p);break;
case 19:fg166_launch_kind<0,19>(p);break;
case 20:fg166_launch_kind<0,20>(p);break;
case 21:fg166_launch_kind<0,21>(p);break;
case 22:fg166_launch_kind<0,22>(p);break;
case 23:fg166_launch_kind<0,23>(p);break;
case 24:fg166_launch_kind<0,24>(p);break;
case 25:fg166_launch_kind<0,25>(p);break;
case 26:fg166_launch_kind<0,26>(p);break;
default:GGML_ABORT("Invalid CUDA unary op");}break;
case 1:switch(p->plan.iv[21]){
case 0:fg166_launch_kind<1,0>(p);break;
case 1:fg166_launch_kind<1,1>(p);break;
case 2:fg166_launch_kind<1,2>(p);break;
case 3:fg166_launch_kind<1,3>(p);break;
case 4:fg166_launch_kind<1,4>(p);break;
case 5:fg166_launch_kind<1,5>(p);break;
case 6:fg166_launch_kind<1,6>(p);break;
case 7:fg166_launch_kind<1,7>(p);break;
case 8:fg166_launch_kind<1,8>(p);break;
case 9:fg166_launch_kind<1,9>(p);break;
case 10:fg166_launch_kind<1,10>(p);break;
case 11:fg166_launch_kind<1,11>(p);break;
case 12:fg166_launch_kind<1,12>(p);break;
case 13:fg166_launch_kind<1,13>(p);break;
case 14:fg166_launch_kind<1,14>(p);break;
case 15:fg166_launch_kind<1,15>(p);break;
case 16:fg166_launch_kind<1,16>(p);break;
case 17:fg166_launch_kind<1,17>(p);break;
case 18:fg166_launch_kind<1,18>(p);break;
case 19:fg166_launch_kind<1,19>(p);break;
case 20:fg166_launch_kind<1,20>(p);break;
case 21:fg166_launch_kind<1,21>(p);break;
case 22:fg166_launch_kind<1,22>(p);break;
case 23:fg166_launch_kind<1,23>(p);break;
case 24:fg166_launch_kind<1,24>(p);break;
case 25:fg166_launch_kind<1,25>(p);break;
default:GGML_ABORT("Invalid CUDA strided unary op");}break;
case 2:switch(p->plan.iv[21]){
case 4:fg166_launch_kind<2,4>(p);break;
case 5:fg166_launch_kind<2,5>(p);break;
case 6:fg166_launch_kind<2,6>(p);break;
case 7:fg166_launch_kind<2,7>(p);break;
case 9:fg166_launch_kind<2,9>(p);break;
case 10:fg166_launch_kind<2,10>(p);break;
case 25:fg166_launch_kind<2,25>(p);break;
default:GGML_ABORT("Invalid CUDA gated unary op");}break;
case 3:fg166_launch_kind<3,0>(p);break;
case 4:fg166_launch_kind<4,0>(p);break;
case 5:fg166_launch_kind<5,0>(p);break;
case 6:fg166_launch_kind<6,0>(p);break;
case 7:fg166_launch_kind<7,0>(p);break;
case 8:fg166_launch_kind<8,0>(p);break;
case 9:fg166_launch_kind<9,0>(p);break;
default:GGML_ABORT("Invalid CUDA activation mode");}}
extern "C" __attribute__((visibility("default"))) uint64_t fg166_cuda_host_seen(){return fg166_seen.load();}
extern "C" __attribute__((visibility("default"))) int fg166_cuda_device_read(uint64_t*out){return(int)cudaMemcpyFromSymbol(out,fg166_device_modes,sizeof(fg166_device_modes));}
static void fg166_report(){uint64_t seen=fg166_seen.load();if(!seen)return;uint64_t device[10]={};int status=(int)cudaMemcpyFromSymbol(device,fg166_device_modes,sizeof(fg166_device_modes));fprintf(stderr,"FG166_CUDA_ACTIVATIONS host=0x%" PRIx64 ",cuda_status=%d",seen,status);for(int i=0;i<10;i++)fprintf(stderr,",m%d=0x%" PRIx64,i,device[i]);fputc('\n',stderr);}
void ggml_cuda_op_abs(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_abs(ctx,a);return;}fg166_run(0,ctx,a);}
void ggml_cuda_op_sgn(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_sgn(ctx,a);return;}fg166_run(1,ctx,a);}
void ggml_cuda_op_neg(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_neg(ctx,a);return;}fg166_run(2,ctx,a);}
void ggml_cuda_op_step(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_step(ctx,a);return;}fg166_run(3,ctx,a);}
void ggml_cuda_op_gelu(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_gelu(ctx,a);return;}fg166_run(4,ctx,a);}
void ggml_cuda_op_gelu_erf(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_gelu_erf(ctx,a);return;}fg166_run(5,ctx,a);}
void ggml_cuda_op_gelu_quick(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_gelu_quick(ctx,a);return;}fg166_run(6,ctx,a);}
void ggml_cuda_op_silu(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_silu(ctx,a);return;}fg166_run(7,ctx,a);}
void ggml_cuda_op_tanh(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_tanh(ctx,a);return;}fg166_run(8,ctx,a);}
void ggml_cuda_op_relu(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_relu(ctx,a);return;}fg166_run(9,ctx,a);}
void ggml_cuda_op_sigmoid(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_sigmoid(ctx,a);return;}fg166_run(10,ctx,a);}
void ggml_cuda_op_hardsigmoid(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_hardsigmoid(ctx,a);return;}fg166_run(11,ctx,a);}
void ggml_cuda_op_hardswish(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_hardswish(ctx,a);return;}fg166_run(12,ctx,a);}
void ggml_cuda_op_exp(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_exp(ctx,a);return;}fg166_run(13,ctx,a);}
void ggml_cuda_op_sqr(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_sqr(ctx,a);return;}fg166_run(14,ctx,a);}
void ggml_cuda_op_sqrt(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_sqrt(ctx,a);return;}fg166_run(15,ctx,a);}
void ggml_cuda_op_sin(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_sin(ctx,a);return;}fg166_run(16,ctx,a);}
void ggml_cuda_op_cos(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_cos(ctx,a);return;}fg166_run(17,ctx,a);}
void ggml_cuda_op_log(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_log(ctx,a);return;}fg166_run(18,ctx,a);}
void ggml_cuda_op_elu(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_elu(ctx,a);return;}fg166_run(19,ctx,a);}
void ggml_cuda_op_floor(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_floor(ctx,a);return;}fg166_run(20,ctx,a);}
void ggml_cuda_op_ceil(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_ceil(ctx,a);return;}fg166_run(21,ctx,a);}
void ggml_cuda_op_round(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_round(ctx,a);return;}fg166_run(22,ctx,a);}
void ggml_cuda_op_trunc(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_trunc(ctx,a);return;}fg166_run(23,ctx,a);}
void ggml_cuda_op_expm1(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_expm1(ctx,a);return;}fg166_run(24,ctx,a);}
void ggml_cuda_op_softplus(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_softplus(ctx,a);return;}fg166_run(25,ctx,a);}
void ggml_cuda_op_reglu(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_reglu(ctx,a);return;}fg166_run(26,ctx,a);}
void ggml_cuda_op_geglu(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_geglu(ctx,a);return;}fg166_run(27,ctx,a);}
void ggml_cuda_op_swiglu(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_swiglu(ctx,a);return;}fg166_run(28,ctx,a);}
void ggml_cuda_op_geglu_erf(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_geglu_erf(ctx,a);return;}fg166_run(29,ctx,a);}
void ggml_cuda_op_geglu_quick(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_geglu_quick(ctx,a);return;}fg166_run(30,ctx,a);}
void ggml_cuda_op_swiglu_oai(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_swiglu_oai(ctx,a);return;}fg166_run(31,ctx,a);}
void ggml_cuda_op_swiglu_clamp(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_swiglu_clamp(ctx,a);return;}fg166_run(32,ctx,a);}
void ggml_cuda_op_xielu(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_xielu(ctx,a);return;}fg166_run(33,ctx,a);}
void ggml_cuda_op_silu_back(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_silu_back(ctx,a);return;}fg166_run(34,ctx,a);}
void ggml_cuda_op_leaky_relu(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_leaky_relu(ctx,a);return;}fg166_run(35,ctx,a);}
void ggml_cuda_op_scale(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_scale(ctx,a);return;}fg166_run(36,ctx,a);}
void ggml_cuda_op_clamp(ggml_backend_cuda_context&ctx,ggml_tensor *a){if(!fg166_enabled()){fg166_native_ggml_cuda_op_clamp(ctx,a);return;}fg166_run(37,ctx,a);}
void ggml_cuda_op_unary_mul(ggml_backend_cuda_context&ctx,ggml_tensor *a,ggml_tensor *b){if(!fg166_enabled()){fg166_native_ggml_cuda_op_unary_mul(ctx,a,b);return;}fg166_run(38,ctx,a,b);}
void ggml_cuda_op_relu_sqr(ggml_backend_cuda_context&ctx,ggml_tensor *a,ggml_tensor *b){if(!fg166_enabled()){fg166_native_ggml_cuda_op_relu_sqr(ctx,a,b);return;}fg166_run(39,ctx,a,b);}
