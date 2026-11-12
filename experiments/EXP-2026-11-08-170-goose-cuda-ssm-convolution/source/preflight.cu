#include "ssm-conv.cuh"
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
static ggml_backend_cuda_context*active;
static void reject(const char*){_exit(active->streams[0][0]==nullptr?81:82);}
static void shape(ggml_tensor&t,int64_t a,int64_t b,int64_t c,uint64_t data){memset(&t,0,sizeof t);t.type=GGML_TYPE_F32;t.ne[0]=a;t.ne[1]=b;t.ne[2]=c;t.ne[3]=1;t.nb[0]=4;t.nb[1]=a*4;t.nb[2]=a*b*4;t.nb[3]=a*b*c*4;t.data=(void*)(uintptr_t)data;}
int main(){for(int test=0;test<26;test++){pid_t pid=fork();if(pid<0)return 2;if(pid==0){ggml_backend_cuda_context ctx(0);active=&ctx;ggml_set_abort_callback(reject);ggml_tensor x,w,d,s,b,add;shape(x,36,128,2,0x1000000);shape(w,4,128,1,0x2000000);shape(d,128,33,2,0x3000000);shape(s,128,33,2,0x4000000);shape(b,128,1,1,0x5000000);memset(&add,0,sizeof add);d.src[0]=&x;d.src[1]=&w;add.src[0]=&d;add.src[1]=&b;ggml_tensor*dp=&d,*ap=nullptr,*sp=nullptr;switch(test){
case 0:dp=nullptr;break;case 1:d.src[0]=nullptr;break;case 2:d.src[1]=nullptr;break;case 3:x.type=GGML_TYPE_F16;break;case 4:w.type=GGML_TYPE_F16;break;case 5:d.type=GGML_TYPE_I32;break;case 6:x.data=nullptr;break;case 7:d.data=nullptr;break;case 8:x.nb[0]=8;break;case 9:x.nb[1]+=4;break;case 10:shape(w,6,128,1,0x2000000);break;case 11:shape(x,36,127,2,0x1000000);shape(d,127,33,2,0x3000000);break;case 12:d.ne[0]=127;break;case 13:x.ne[0]=35;x.nb[1]=140;break;case 14:x.ne[2]=1;break;case 15:w.ne[1]=127;break;case 16:w.nb[1]=8;break;case 17:d.nb[0]=8;break;case 18:d.nb[1]=4;break;case 19:ap=&add;break;case 20:ap=&add;sp=&s;b.type=GGML_TYPE_F16;break;case 21:ap=&add;sp=&s;b.ne[0]=127;break;case 22:x.data=(void*)(uintptr_t)(UINT64_MAX-16);break;case 23:d.data=x.data;break;case 24:x.nb[1]=(size_t)INT64_MAX;break;case 25:d.ne[1]=65535LL*32+1;break;
}ggml_cuda_op_ssm_conv(ctx,dp,ap,sp);_exit(86);}int st=0;if(waitpid(pid,&st,0)!=pid||!WIFEXITED(st)||WEXITSTATUS(st)!=81){fprintf(stderr,"FAIL SSM preflight case=%d status=%d\n",test,st);return 3;}}puts("PASS SSM preflight_cases=26 no_stream_or_kernel_before_rejection=1");}
