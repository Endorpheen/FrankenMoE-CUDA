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
static void graph_case(int width,int rows,int multi,int inject){
 CHECK(live==0);calls=freed=init_calls=fail_alloc=fail_init=0;limit=multi?4096:65536;
 struct ggml_context*c=ggml_init((struct ggml_init_params){262144,NULL,true});
 struct ggml_tensor*x=ggml_new_tensor_2d(c,GGML_TYPE_F32,width,rows);ggml_set_input(x);struct ggml_tensor*y=ggml_new_tensor_2d(c,GGML_TYPE_F32,width,rows);ggml_set_input(y);
 struct ggml_tensor*sum=ggml_add(c,x,y);struct ggml_tensor*scaled=ggml_scale(c,sum,0.5f);struct ggml_tensor*v=ggml_view_2d(c,scaled,width/2,rows,scaled->nb[1],0);struct ggml_tensor*z=ggml_cont(c,v);ggml_set_output(z);
 struct ggml_cgraph*g=ggml_new_graph(c);ggml_build_forward_expand(g,z);
 ggml_backend_buffer_type_t types[]={&type,&type};ggml_gallocr_t ga=multi?ggml_gallocr_new_n(types,2):ggml_gallocr_new(&type);CHECK(ga);
 size_t limits[]={32768,16384};ggml_gallocr_set_buffer_growth_limits(ga,limits);
 int ids[64]={0};for(int i=0;i<ggml_graph_n_nodes(g);i++)ids[i]=multi?i%2:0;
 if(inject)fail_alloc=1;
 bool reserved=ggml_gallocr_reserve_n(ga,g,ids,NULL);if(inject&&reserved){CHECK(calls>1);}mix(reserved);
 if(!reserved){CHECK(inject||multi);fail_alloc=0;reserved=ggml_gallocr_reserve_n(ga,g,ids,NULL);}
 if(reserved){CHECK(ggml_gallocr_alloc_graph(ga,g));mix(ggml_gallocr_get_buffer_size(ga,0));if(multi)mix(ggml_gallocr_get_buffer_size(ga,1));
  for(int i=0;i<ggml_graph_n_nodes(g);i++){struct ggml_tensor*t=ggml_graph_node(g,i);CHECK(t->data&&t->buffer);mix(ggml_nbytes(t));mix((uintptr_t)t->data-(uintptr_t)ggml_backend_buffer_get_base(t->buffer));}
  CHECK(z->data&&z->buffer);memset(z->data,0x6b,ggml_nbytes(z));CHECK(((unsigned char*)z->data)[ggml_nbytes(z)-1]==0x6b);CHECK(ggml_gallocr_alloc_graph(ga,g));
 }
 ggml_gallocr_free(ga);CHECK(live==0);ggml_free(c);cases++;
}
int main(void){for(int width=16;width<=64;width*=2)for(int rows=1;rows<=16;rows*=4)for(int multi=0;multi<2;multi++)for(int inject=0;inject<2;inject++)graph_case(width,rows,multi,inject);printf("{\"status\":\"PASS_LOCAL_GRAPH_MEMORY\",\"cases\":%u,\"hash\":\"%016llx\"}\n",cases,(unsigned long long)hash);return 0;}
