#include "norm.cuh"
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
static ggml_backend_cuda_context*active;
static void reject(const char*){_exit(active->streams[0][0]==nullptr?81:82);}
static void shape(ggml_tensor&t){std::memset(&t,0,sizeof t);t.type=GGML_TYPE_F32;t.ne[0]=19;t.ne[1]=3;t.ne[2]=2;t.ne[3]=1;t.nb[0]=4;t.nb[1]=76;t.nb[2]=228;t.nb[3]=456;t.data=(void*)(uintptr_t)0x1000;}
int main(){for(int test=0;test<24;test++){pid_t pid=fork();if(pid<0)return 2;if(pid==0){ggml_backend_cuda_context ctx(0);active=&ctx;ggml_set_abort_callback(reject);ggml_tensor x,d,m,a,grad;shape(x);shape(d);shape(m);shape(a);shape(grad);d.src[0]=&x;m.src[0]=&d;m.src[1]=&grad;a.src[0]=&m;a.src[1]=&x;float eps=1e-5f;memcpy(d.op_params,&eps,4);switch(test){
case 0:d.src[0]=nullptr;break;
case 1:x.data=nullptr;break;
case 2:x.nb[0]=8;break;
case 3:x.ne[0]=0;break;
case 4:x.ne[0]=INT64_C(2147483648);break;
case 5:x.nb[3]=SIZE_MAX;break;
case 6:x.ne[2]=2147483647;x.nb[2]=INT64_C(9223372036854775804);break;
case 7:d.type=GGML_TYPE_F16;break;
case 8:d.ne[1]=2;break;
case 9:eps=-1;memcpy(d.op_params,&eps,4);break;
case 10:m.src[1]=nullptr;ggml_cuda_op_rms_norm_fused(ctx,&d,&m);_exit(86);
case 11:ggml_cuda_op_rms_norm_fused(ctx,&d,nullptr);_exit(86);
case 12:m.src[0]=&x;ggml_cuda_op_rms_norm_fused(ctx,&d,&m);_exit(86);
case 13:d.ne[1]=2;ggml_cuda_op_rms_norm_fused(ctx,&d,&m);_exit(86);
case 14:m.type=GGML_TYPE_F16;ggml_cuda_op_rms_norm_fused_add(ctx,&d,&m,&a);_exit(86);
case 15:a.src[1]=nullptr;ggml_cuda_op_rms_norm_fused_add(ctx,&d,&m,&a);_exit(86);
case 16:eps=1;memcpy((char*)m.op_params+4,&eps,4);ggml_cuda_op_rms_norm_scale_fused(ctx,&d,&m);_exit(86);
case 17:d.src[0]=&grad;d.src[1]=&x;grad.nb[1]+=4;ggml_cuda_op_rms_norm_back(ctx,&d);_exit(86);
case 18:d.src[0]=&grad;d.src[1]=&x;grad.ne[1]=2;ggml_cuda_op_rms_norm_back(ctx,&d);_exit(86);
case 19:{int groups=0;memcpy(d.op_params,&groups,4);memcpy((char*)d.op_params+4,&eps,4);ggml_cuda_op_group_norm(ctx,&d);_exit(86);}
case 20:{int groups=2;x.nb[1]+=4;memcpy(d.op_params,&groups,4);ggml_cuda_op_group_norm(ctx,&d);_exit(86);}
case 21:m.ne[0]=0;ggml_cuda_op_rms_norm_fused(ctx,&d,&m);_exit(86);
case 22:d.src[0]=&grad;d.src[1]=nullptr;ggml_cuda_op_rms_norm_back(ctx,&d);_exit(86);
case 23:m.ne[1]=2;ggml_cuda_op_rms_norm_fused_add(ctx,&d,&m,&a);_exit(86);
}ggml_cuda_op_rms_norm(ctx,&d);_exit(86);}int st=0;if(waitpid(pid,&st,0)!=pid||!WIFEXITED(st)||WEXITSTATUS(st)!=81){fprintf(stderr,"FAIL normalization preflight case=%d status=%d\n",test,st);return 3;}}puts("PASS normalization preflight_cases=24 no_stream_or_kernel_effect_before_rejection=1");}
