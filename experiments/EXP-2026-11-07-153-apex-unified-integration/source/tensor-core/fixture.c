#include "ggml.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(2);}}while(0)
static uint64_t hash=1469598103934665603ULL;static unsigned cases;
static void mix(uint64_t v){hash^=v;hash*=1099511628211ULL;}
static void tensor_record(struct ggml_tensor*t){mix(t->type);mix(t->op);mix(t->flags);mix(t->view_offs);mix(t->data!=0);mix(t->view_src!=0);for(int i=0;i<4;i++){mix(t->ne[i]);mix(t->nb[i]);}mix(ggml_nbytes(t));mix(ggml_nelements(t));mix(ggml_nrows(t));cases++;}
static void *worker(void*arg){(void)arg;for(int i=0;i<24;i++){struct ggml_context*c=ggml_init((struct ggml_init_params){8192,NULL,false});struct ggml_tensor*t=ggml_new_tensor_2d(c,GGML_TYPE_F32,32,8);CHECK(t&&t->data&&ggml_nbytes(t)==1024);ggml_free(c);}return NULL;}
static void negative(int kind){struct ggml_context*c=ggml_init((struct ggml_init_params){8192,NULL,true});struct ggml_tensor*t=ggml_new_tensor_1d(c,GGML_TYPE_F32,32);if(kind==0){ggml_view_1d(c,t,32,4);}else if(kind==1){int64_t shape[2]={INT64_MAX/2,4};ggml_new_tensor(c,GGML_TYPE_F32,2,shape);}else{ggml_new_tensor(c,GGML_TYPE_COUNT,1,t->ne);}ggml_free(c);_exit(0);}
int main(void){
 pthread_t threads[4];for(int j=0;j<4;j++)CHECK(pthread_create(threads+j,0,worker,0)==0);for(int j=0;j<4;j++)CHECK(pthread_join(threads[j],0)==0);
 const enum ggml_type kinds[]={GGML_TYPE_F32,GGML_TYPE_F16,GGML_TYPE_BF16,GGML_TYPE_I32,GGML_TYPE_Q4_0,GGML_TYPE_Q8_0,GGML_TYPE_Q8_K,GGML_TYPE_Q4_K,GGML_TYPE_Q6_K};
 for(unsigned k=0;k<sizeof(kinds)/sizeof(*kinds);k++)for(int no=0;no<2;no++)for(int external=0;external<2;external++){
  alignas(64) unsigned char pool[262144+128];memset(pool,0xa7,sizeof(pool));size_t cap=262144;
  struct ggml_context*c=ggml_init((struct ggml_init_params){cap,external?pool+64:NULL,no});CHECK(c);CHECK(ggml_get_mem_size(c)==cap);CHECK(ggml_get_no_alloc(c)==no);CHECK(ggml_used_mem(c)==0);CHECK(!ggml_get_first_tensor(c));CHECK(!ggml_get_tensor(c,"missing"));
  int64_t block=ggml_blck_size(kinds[k]);size_t max=0;unsigned count=0;
  for(int dims=1;dims<=4;dims++){
   int64_t shape[4]={block*2,3,2,2};struct ggml_tensor*t=ggml_new_tensor(c,kinds[k],dims,shape);CHECK(t);ggml_set_name(t,"root");CHECK(ggml_get_tensor(c,"root"));tensor_record(t);count++;size_t bytes=ggml_nbytes(t);if(bytes>max)max=bytes;
   if(t->data){memset(t->data,0x5c,bytes);CHECK(((unsigned char*)t->data)[bytes-1]==0x5c);}
   struct ggml_tensor*v=ggml_view_1d(c,t,block,0);CHECK(v->view_src==t);CHECK(v->data==t->data);tensor_record(v);count++;
   struct ggml_tensor*v2=ggml_view_1d(c,v,block/2?block:1,0);CHECK(v2->view_src==t);tensor_record(v2);count++;
   struct ggml_tensor*dup=ggml_dup_tensor(c,t);tensor_record(dup);count++;
   void*scratch=ggml_new_buffer(c,17);CHECK(scratch);memset(scratch,0x9b,17);mix(ggml_used_mem(c));
  }
  CHECK(ggml_get_max_tensor_size(c)==max);unsigned seen=0;for(struct ggml_tensor*t=ggml_get_first_tensor(c);t;t=ggml_get_next_tensor(c,t))seen++;CHECK(seen==count);mix(seen);
  ggml_reset(c);CHECK(ggml_used_mem(c)==0&&!ggml_get_first_tensor(c));ggml_set_no_alloc(c,true);struct ggml_tensor*z=ggml_new_tensor_2d(c,kinds[k],block,0);CHECK(ggml_nbytes(z)==0&&!z->data);tensor_record(z);ggml_free(c);
  if(external)for(int i=0;i<64;i++){CHECK(pool[i]==0xa7);CHECK(pool[262144+64+i]==0xa7);}
 }
 struct ggml_context*c=ggml_init((struct ggml_init_params){262144,NULL,false});struct ggml_tensor*a=ggml_new_tensor_2d(c,GGML_TYPE_F32,64,16);struct ggml_tensor*b=ggml_new_tensor_2d(c,GGML_TYPE_F32,64,16);struct ggml_tensor*sum=ggml_add(c,a,b);struct ggml_tensor*v=ggml_view_2d(c,sum,32,8,sum->nb[1],4*sizeof(float));struct ggml_tensor*n=ggml_view_1d(c,v,8,4*sizeof(float));CHECK(n->view_src==sum&&n->view_offs==8*sizeof(float));tensor_record(n);struct ggml_cgraph*g=ggml_new_graph(c);ggml_build_forward_expand(g,n);mix(ggml_graph_n_nodes(g));ggml_free(c);ggml_reset(NULL);ggml_free(NULL);
 c=ggml_init((struct ggml_init_params){0,NULL,true});CHECK(c&&ggml_get_mem_size(c)>0);mix(ggml_get_mem_size(c));ggml_free(c);
 for(int i=0;i<3;i++){pid_t p=fork();CHECK(p>=0);if(!p)negative(i);int status;CHECK(waitpid(p,&status,0)==p);CHECK(WIFSIGNALED(status)&&WTERMSIG(status)==SIGABRT);}
 printf("{\"status\":\"PASS_LOCAL_TENSOR_CONTEXT\",\"tensor_cases\":%u,\"hash\":\"%016llx\",\"thread_contexts\":96,\"rejected_inputs\":3}\n",cases,(unsigned long long)hash);return 0;}
