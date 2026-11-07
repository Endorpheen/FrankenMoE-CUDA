#define _GNU_SOURCE
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>
#include <errno.h>
static int failure_kind=-1,failure_start=-1,starts,allocs[2],frees[2],live[2];
void * fg149_system_alloc(int64_t kind,int64_t bytes){if(kind<0||kind>1||bytes<=0)abort();if(kind==failure_kind)return NULL;void*p=NULL;if(posix_memalign(&p,64,(size_t)bytes))abort();++allocs[kind];++live[kind];return p;}
void fg149_system_free(int64_t kind,void*p,int64_t bytes){(void)bytes;if(!p)return;if(kind<0||kind>1||live[kind]<=0)abort();--live[kind];++frees[kind];free(p);}
int64_t fg149_system_start(void*worker){if(starts++==failure_start)return EAGAIN;int64_t(*real_start)(void*)=(int64_t(*)(void*))dlsym(RTLD_NEXT,"fg149_system_start");if(!real_start)abort();return real_start(worker);}
static void check(int ok){if(!ok)abort();}
static void no_leaks(void){check(live[0]==0&&live[1]==0);}
static bool stop_graph(void*value){++*(int*)value;return true;}
int main(int argc,char**argv){int threaded=argc>1&&!strcmp(argv[1],"pthread");int cases=0;ggml_cpu_init();
 {struct ggml_init_params init={2*1024*1024,NULL,false};struct ggml_context*ctx=ggml_init(init);check(ctx!=NULL);struct ggml_tensor*x=ggml_new_tensor_2d(ctx,GGML_TYPE_F32,16,4),*y=ggml_dup(ctx,x);struct ggml_cgraph*g=ggml_new_graph(ctx);ggml_build_forward_expand(g,y);struct ggml_cplan plan=ggml_graph_plan(g,2,NULL);check(plan.work_size==0);
  for(int k=0;k<2;++k){failure_kind=k;check(ggml_graph_compute(g,&plan)==GGML_STATUS_ALLOC_FAILED);no_leaks();++cases;}failure_kind=-1;ggml_free(ctx);}

 for(int kind=0;kind<2;++kind){failure_kind=kind;struct ggml_threadpool_params cfg=ggml_threadpool_params_default(4);struct ggml_threadpool*p=ggml_threadpool_new(&cfg);check(!p);no_leaks();++cases;}failure_kind=-1;
 if(threaded)for(int at=0;at<3;++at){failure_start=at;starts=0;struct ggml_threadpool_params cfg=ggml_threadpool_params_default(4);cfg.paused=true;struct ggml_threadpool*p=ggml_threadpool_new(&cfg);check(!p);no_leaks();++cases;}failure_start=-1;
 for(int count=1;count<=16;count=count==1?2:count==2?8:16){
  for(int paused=0;paused<2;++paused){struct ggml_threadpool_params cfg=ggml_threadpool_params_default(count);cfg.poll=0;cfg.paused=paused;struct ggml_threadpool*pool=ggml_threadpool_new(&cfg);check(pool!=NULL);ggml_threadpool_pause(pool);ggml_threadpool_pause(pool);ggml_threadpool_resume(pool);ggml_threadpool_resume(pool);
   struct ggml_init_params init={4*1024*1024,NULL,false};struct ggml_context*ctx=ggml_init(init);check(ctx!=NULL);struct ggml_tensor*x=ggml_new_tensor_2d(ctx,GGML_TYPE_F32,128,32),*y=ggml_new_tensor_2d(ctx,GGML_TYPE_F32,128,32);struct ggml_tensor*z=ggml_mul(ctx,x,y);struct ggml_cgraph*g=ggml_new_graph(ctx);ggml_build_forward_expand(g,z);
   struct ggml_cplan plan=ggml_graph_plan(g,count,pool);check(plan.work_size==0);int aborts=0;
   for(int iteration=0;iteration<8;++iteration){for(int i=0;i<4096;++i){((float*)x->data)[i]=(float)(i%97+iteration)*.125f;((float*)y->data)[i]=.5f;((float*)z->data)[i]=0;}
    plan.abort_callback=iteration==3?stop_graph:NULL;plan.abort_callback_data=&aborts;enum ggml_status status=ggml_graph_compute(g,&plan);check(status==(iteration==3?GGML_STATUS_ABORTED:GGML_STATUS_SUCCESS));for(int i=0;i<4096;++i)check(((float*)z->data)[i]==((float*)x->data)[i]*.5f);++cases;
    if(iteration==4){ggml_threadpool_pause(pool);ggml_threadpool_resume(pool);}
   }
   check(aborts==1);ggml_free(ctx);ggml_threadpool_free(pool);no_leaks();++cases;
  }
  if(count==16)break;
 }
 struct ggml_threadpool_params cfg=ggml_threadpool_params_default(0);check(ggml_threadpool_new(&cfg)==NULL);++cases;cfg.n_threads=513;check(ggml_threadpool_new(&cfg)==NULL);++cases;ggml_threadpool_free(NULL);no_leaks();
 printf("{\"status\":\"PASS_REAL_POOL_LIFECYCLE_FAILURE_REUSE_ABORT\",\"cases\":%d,\"pthread\":%s,\"pool_allocs\":%d,\"pool_frees\":%d,\"worker_allocs\":%d,\"worker_frees\":%d,\"scope\":\"Real graph/runtime/threads with injected system allocation or start failures; not OS OOM\"}\n",cases,threaded?"true":"false",allocs[0],frees[0],allocs[1],frees[1]);return 0;}
