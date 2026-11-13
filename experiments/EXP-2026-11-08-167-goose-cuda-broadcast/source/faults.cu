#include "binbcast.cuh"
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
static ggml_backend_cuda_context*active;
static void reject(const char*){_exit(active->streams[0][0]==nullptr?81:82);}
static void shape(ggml_tensor&t){std::memset(&t,0,sizeof t);t.type=GGML_TYPE_F32;t.ne[0]=19;t.ne[1]=t.ne[2]=t.ne[3]=1;t.nb[0]=4;t.nb[1]=t.nb[2]=t.nb[3]=76;t.data=(void*)(uintptr_t)0x1000;}
int main(){for(int test=0;test<12;test++){pid_t p=fork();if(p<0)return 2;if(p==0){ggml_backend_cuda_context ctx(0);active=&ctx;ggml_set_abort_callback(reject);ggml_tensor a,b,c,d;shape(a);shape(b);shape(c);shape(d);d.src[0]=&a;d.src[1]=&b;d.src[2]=&c;switch(test){case 0:d.src[0]=nullptr;break;case 1:d.src[1]=nullptr;break;case 2:a.type=GGML_TYPE_BF16;break;case 3:b.ne[0]=7;break;case 4:ggml_cuda_op_fused_add(ctx,&d,9);_exit(86);case 5:d.src[2]=nullptr;ggml_cuda_op_fused_add(ctx,&d,2);_exit(86);case 6:c.type=GGML_TYPE_F16;ggml_cuda_op_fused_mul(ctx,&d,2);_exit(86);case 7:b.nb[0]=3;break;case 8:d.ne[0]=0;break;case 9:d.ne[0]=17;ggml_cuda_op_repeat_back(ctx,&d);_exit(86);case 10:d.ne[2]=a.ne[2]=UINT32_MAX;d.ne[3]=a.ne[3]=2;b.ne[0]=1;break;case 11:a.nb[3]=SIZE_MAX;a.ne[3]=d.ne[3]=2;break;}ggml_cuda_op_add(ctx,&d);_exit(86);}int st=0;if(waitpid(p,&st,0)!=p||!WIFEXITED(st)||WEXITSTATUS(st)!=81){fprintf(stderr,"FAIL broadcast preflight case=%d status=%d\n",test,st);return 3;}}puts("PASS broadcast preflight_cases=12 no_stream_or_kernel_effect_before_rejection=1");}
