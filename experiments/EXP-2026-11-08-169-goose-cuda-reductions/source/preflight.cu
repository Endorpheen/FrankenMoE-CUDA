#include "sumrows.cuh"
#include "mean.cuh"
#include "argmax.cuh"
#include "sum.cuh"
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
static ggml_backend_cuda_context*active;
static void reject(const char*){_exit(active->streams[0][0]==nullptr?81:82);}
static void shape(ggml_tensor&t,int64_t cols,int64_t rows,uint64_t ptr){memset(&t,0,sizeof t);t.type=GGML_TYPE_F32;t.ne[0]=cols;t.ne[1]=rows;t.ne[2]=t.ne[3]=1;t.nb[0]=4;t.nb[1]=cols*4;t.nb[2]=t.nb[3]=t.nb[1]*rows;t.data=(void*)(uintptr_t)ptr;}
struct DummyPool:ggml_cuda_pool{void*alloc(size_t,size_t*)override{_exit(83);}void free(void*,size_t)override{_exit(83);}};
int main(){for(int test=0;test<24;test++){pid_t pid=fork();if(pid<0)return 2;if(pid==0){ggml_backend_cuda_context ctx(0);active=&ctx;ggml_set_abort_callback(reject);ggml_tensor x,d;shape(x,19,3,0x1000);shape(d,3,1,UINT64_C(0x200000000000));d.src[0]=&x;int mode=0;switch(test){
case 0:d.src[0]=nullptr;break;case 1:x.data=nullptr;break;case 2:d.data=nullptr;break;case 3:x.ne[0]=0;break;case 4:x.nb[1]=SIZE_MAX;break;case 5:x.type=GGML_TYPE_F16;break;case 6:d.type=GGML_TYPE_I32;break;case 7:x.nb[0]=8;break;case 8:d.ne[0]=2;break;case 9:d.nb[0]=8;break;case 10:d.data=x.data;break;case 11:x.ne[0]=INT64_C(2147483648);break;
case 12:x.ne[1]=INT64_C(2147483648);d.ne[0]=INT64_C(2147483648);break;
case 13:mode=2;d.type=GGML_TYPE_I32;x.nb[1]+=4;break;
case 14:mode=2;break;
case 15:mode=2;d.type=GGML_TYPE_I32;d.ne[0]=2;break;
case 16:mode=4;d.ne[0]=1;x.nb[1]+=4;break;
case 17:mode=4;d.ne[0]=2;break;
case 18:x.ne[0]=INT64_MAX/4;x.ne[1]=16;break;
case 19:sum_rows_f32_cuda((const float*)x.data,(float*)d.data,0,1,nullptr);_exit(86);
case 20:{DummyPool pool;sum_f32_cuda(pool,(const float*)x.data,(float*)d.data,0,nullptr);_exit(86);}
case 21:sum_rows_f32_cuda((const float*)x.data,(float*)x.data,19,3,nullptr);_exit(86);
case 22:x.nb[1]=INT64_MAX-3;x.ne[1]=4;d.ne[0]=4;break;
case 23:mode=2;d.type=GGML_TYPE_I32;shape(x,INT64_C(2147483648),3,0x1000);break;
}if(mode==2)ggml_cuda_argmax(ctx,&d);else if(mode==4)ggml_cuda_op_sum(ctx,&d);else ggml_cuda_op_sum_rows(ctx,&d);_exit(86);}int st=0;if(waitpid(pid,&st,0)!=pid||!WIFEXITED(st)||WEXITSTATUS(st)!=81){fprintf(stderr,"FAIL reduction preflight case=%d status=%d\n",test,st);return 3;}}puts("PASS reduction preflight_cases=24 no_stream_or_kernel_or_pool_effect_before_rejection=1");}
