#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend-impl.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(2);}}while(0)
static size_t limit=1024;static int calls,live,freed,fail_alloc,fail_init,init_calls;
static uint64_t hash=1469598103934665603ULL;static unsigned cases;
static void mix(uint64_t v){hash^=v;hash*=1099511628211ULL;}
static const char*name(ggml_backend_buffer_type_t t){(void)t;return "fg153-fixture-host";}
static size_t alignment(ggml_backend_buffer_type_t t){(void)t;return 64;}
static size_t max_size(ggml_backend_buffer_type_t t){(void)t;return limit;}
static void release(ggml_backend_buffer_t b){CHECK(live>0);free(b->context);live--;freed++;}
static void*base(ggml_backend_buffer_t b){return b->context;}
static enum ggml_status init(ggml_backend_buffer_t b,struct ggml_tensor*t){(void)b;init_calls++;t->extra=(void*)(uintptr_t)(0x123400+init_calls);if(fail_init==init_calls)return GGML_STATUS_FAILED;CHECK(t->data);return GGML_STATUS_SUCCESS;}
static void clear(ggml_backend_buffer_t b,uint8_t v){memset(b->context,v,b->size);}
static ggml_backend_buffer_t alloc(ggml_backend_buffer_type_t t,size_t size){calls++;if(calls==fail_alloc||size>limit)return NULL;void*p=NULL;if(posix_memalign(&p,64,size))return NULL;struct ggml_backend_buffer_i i={0};i.free_buffer=release;i.get_base=base;i.init_tensor=init;i.clear=clear;ggml_backend_buffer_t b=ggml_backend_buffer_init(t,i,p,size);live++;return b;}
static struct ggml_backend_buffer_type type={.iface={.get_name=name,.alloc_buffer=alloc,.get_alignment=alignment,.get_max_size=max_size}};
static struct ggml_context*create(struct ggml_tensor**roots,int n){struct ggml_context*c=ggml_init((struct ggml_init_params){262144,NULL,true});for(int i=0;i<n;i++){roots[i]=ggml_new_tensor_1d(c,GGML_TYPE_F32,32+(i%3)*16);ggml_set_name(roots[i],"weight");struct ggml_tensor*v=ggml_view_1d(c,roots[i],16,4);CHECK(v->data==NULL);}return c;}
int main(void){
 for(int n=0;n<=17;n++)for(int split=0;split<2;split++){
  CHECK(live==0);calls=freed=init_calls=fail_alloc=fail_init=0;limit=split?512:65536;struct ggml_tensor*roots[24];struct ggml_context*c=create(roots,n);
  size_t measured=ggml_backend_alloc_ctx_tensors_from_buft_size(c,&type);ggml_backend_buffer_t b=ggml_backend_alloc_ctx_tensors_from_buft(c,&type);CHECK((n==0)==(b==NULL));mix(measured);mix(calls);mix(live);
  if(b){CHECK(measured==ggml_backend_buffer_get_size(b));ggml_backend_buffer_clear(b,0x5c);unsigned seen=0;for(struct ggml_tensor*t=ggml_get_first_tensor(c);t;t=ggml_get_next_tensor(c,t)){CHECK(t->data&&t->buffer);if(t->view_src){CHECK(t->data==(char*)t->view_src->data+t->view_offs);CHECK(t->buffer==t->view_src->buffer);}else{CHECK((uintptr_t)t->data%64==0);CHECK(((unsigned char*)t->data)[ggml_nbytes(t)-1]==0x5c);}mix(ggml_nbytes(t));mix((uintptr_t)t->data-(uintptr_t)ggml_backend_buffer_get_base(t->buffer));seen++;}mix(seen);CHECK(ggml_backend_alloc_ctx_tensors_from_buft_size(c,&type)==0);CHECK(ggml_backend_alloc_ctx_tensors_from_buft(c,&type)==NULL);ggml_backend_buffer_free(b);CHECK(live==0);}
  mix(freed);ggml_free(c);cases++;
 }
 const char*mode=getenv("GGML_GOOSE_BUFFERS");if(mode&&strcmp(mode,"0")!=0){
  for(int phase=0;phase<2;phase++)for(int at=1;at<=4;at++){
   CHECK(live==0);calls=freed=init_calls=fail_alloc=fail_init=0;limit=256;if(phase)fail_init=at;else fail_alloc=at;
   struct ggml_tensor*roots[24];struct ggml_context*c=create(roots,12);for(struct ggml_tensor*t=ggml_get_first_tensor(c);t;t=ggml_get_next_tensor(c,t))t->extra=(void*)0x100;ggml_backend_buffer_t b=ggml_backend_alloc_ctx_tensors_from_buft(c,&type);CHECK(!b);CHECK(live==0);
   for(struct ggml_tensor*t=ggml_get_first_tensor(c);t;t=ggml_get_next_tensor(c,t))CHECK(!t->data&&!t->buffer&&t->extra==(void*)0x100);
   fail_alloc=fail_init=0;b=ggml_backend_alloc_ctx_tensors_from_buft(c,&type);CHECK(b);ggml_backend_buffer_free(b);CHECK(live==0);ggml_free(c);
  }
 }
 printf("{\"status\":\"PASS_LOCAL_BUFFER_DIFFERENTIAL\",\"cases\":%u,\"hash\":\"%016llx\"}\n",cases,(unsigned long long)hash);return 0;}
