#include "unary.cuh"
#include "scale.cuh"
#include "clamp.cuh"
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
static ggml_backend_cuda_context*active;
static void reject(const char*){_exit(active->streams[0][0]==nullptr?81:82);}
static void shape(ggml_tensor&t){std::memset(&t,0,sizeof t);t.type=GGML_TYPE_F32;t.ne[0]=17;t.ne[1]=t.ne[2]=t.ne[3]=1;t.nb[0]=4;t.nb[1]=t.nb[2]=t.nb[3]=68;t.data=(void*)(uintptr_t)0x1000;}
int main(){for(int test=0;test<8;test++){pid_t p=fork();if(p<0)return 2;if(p==0){ggml_backend_cuda_context ctx(0);active=&ctx;ggml_set_abort_callback(reject);ggml_tensor src,gate,dst;shape(src);shape(gate);shape(dst);dst.src[0]=&src;switch(test){case 0:dst.src[0]=nullptr;ggml_cuda_op_abs(ctx,&dst);break;case 1:src.type=GGML_TYPE_I32;ggml_cuda_op_abs(ctx,&dst);break;case 2:dst.type=GGML_TYPE_F16;ggml_cuda_op_abs(ctx,&dst);break;case 3:src.ne[0]=dst.ne[0]=INT64_C(2147483648);ggml_cuda_op_abs(ctx,&dst);break;case 4:dst.ne[0]=7;ggml_cuda_op_swiglu(ctx,&dst);break;case 5:ggml_cuda_op_silu_back(ctx,&dst);break;case 6:dst.src[1]=&gate;gate.ne[1]=2;ggml_cuda_op_swiglu(ctx,&dst);break;case 7:src.nb[0]=8;ggml_cuda_op_clamp(ctx,&dst);break;}_exit(86);}int st=0;if(waitpid(p,&st,0)!=p||!WIFEXITED(st)||WEXITSTATUS(st)!=81){fprintf(stderr,"FAIL CUDA host preflight test=%d status=%d\n",test,st);return 3;}}puts("PASS CUDA host rejection_cases=8 no_stream_or_kernel_effect_before_rejection=1");}
