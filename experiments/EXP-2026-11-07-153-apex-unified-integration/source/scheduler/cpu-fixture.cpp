#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
extern "C" unsigned long fg153_scheduler_count(int);
extern "C" int fg153_scheduler_validate_layout(void);
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(2);}}while(0)
struct Eval{int asks=0,reads=0;bool stop=false;};
static bool eval(ggml_tensor*t,bool ask,void*p){(void)t;auto*e=(Eval*)p;if(ask){e->asks++;return true;}e->reads++;return !e->stop;}
int main(){
 CHECK(fg153_scheduler_validate_layout()==1);unsigned cases=0;double sum=0;auto cpu=ggml_backend_cpu_init();CHECK(cpu);ggml_backend_cpu_set_n_threads(cpu,1);
 for(int parallel=0;parallel<2;parallel++){
  auto s=ggml_backend_sched_new(&cpu,nullptr,1,2048,parallel!=0,true);CHECK(s);CHECK(ggml_backend_sched_get_n_backends(s)==1);CHECK(ggml_backend_sched_get_n_copies(s)==(parallel?4:1));
  for(int round=0;round<8;round++){
   ggml_backend_sched_reset(s);auto ctx=ggml_init({4*1024*1024,nullptr,true});CHECK(ctx);auto a=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,64);auto b=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,64);ggml_set_input(a);ggml_set_input(b);auto c=ggml_add(ctx,a,b);auto d=ggml_sqr(ctx,c);auto v=ggml_view_1d(ctx,d,32,0);auto z=ggml_add(ctx,v,v);ggml_set_output(z);auto g=ggml_new_graph_custom(ctx,2048,false);ggml_build_forward_expand(g,z);
   size_t measured[1]={};if(round==0){ggml_backend_sched_reserve_size(s,g,measured);CHECK(measured[0]>0);ggml_backend_sched_reset(s);CHECK(ggml_backend_sched_reserve(s,g));}
   CHECK(ggml_backend_sched_alloc_graph(s,g));CHECK(ggml_backend_sched_get_n_splits(s)==1);CHECK(ggml_backend_sched_get_tensor_backend(s,d)==cpu);
   std::vector<float>x(64),y(64);for(int i=0;i<64;i++){x[i]=i*.125f+round;y[i]=.5f;}ggml_backend_tensor_set(a,x.data(),0,256);ggml_backend_tensor_set(b,y.data(),0,256);
   Eval e;if(round==2){ggml_backend_sched_set_eval_callback(s,eval,&e);}CHECK(ggml_backend_sched_graph_compute(s,g)==GGML_STATUS_SUCCESS);std::vector<float>out(32);ggml_backend_tensor_get(z,out.data(),0,128);for(int i=0;i<32;i++){float expected=2*(x[i]+y[i])*(x[i]+y[i]);CHECK(out[i]==expected);sum+=out[i];}if(round==2){CHECK(e.asks>0&&e.reads>0);ggml_backend_sched_set_eval_callback(s,nullptr,nullptr);}
   auto reserved=ggml_backend_sched_get_buffer_size(s,cpu);CHECK(reserved>0);CHECK(ggml_backend_sched_graph_compute_async(s,g)==GGML_STATUS_SUCCESS);ggml_backend_sched_synchronize(s);CHECK(ggml_backend_sched_get_buffer_size(s,cpu)==reserved);ggml_free(ctx);cases++;
  }ggml_backend_sched_free(s);
 }
 const char*mode=getenv("GGML_GOOSE_SCHEDULER");CHECK((fg153_scheduler_count(13)>0)==(!mode||strcmp(mode,"0")!=0));ggml_backend_free(cpu);printf("{\"status\":\"PASS_SCHEDULER_CPU\",\"cases\":%u,\"checksum\":%.0f}\n",cases,sum);
}
