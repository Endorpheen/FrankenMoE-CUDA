#ifndef FG160_SERVICE_H
#define FG160_SERVICE_H
#include <stdatomic.h>
#include <inttypes.h>
#include "service-api.h"
_Static_assert(sizeof(float)==4&&sizeof(int32_t)==4&&sizeof(uintptr_t)==8,"Shared operation ABI");
__attribute__((visibility("default"))) int fg160_fusion_enabled=1;
__attribute__((visibility("default"))) int fg160_graph_enabled=1;
static atomic_uint_fast64_t fg160_seen[5];
static _Thread_local uint64_t fg160_thread_seen[5];
static inline uint64_t fg160_arg(int64_t p,int64_t i){return ((const uint64_t*)(intptr_t)p)[i];}
static inline int64_t fg160_graph(int64_t p,int64_t k){const struct ggml_cgraph*g=(const void*)(intptr_t)p;return k==0?g->n_nodes:g->size;}
static inline int64_t fg160_node(int64_t p,int64_t i){return (int64_t)(intptr_t)((const struct ggml_cgraph*)(intptr_t)p)->nodes[i];}
static inline uint64_t fg160_tensor(int64_t p,int64_t k,int64_t i){const struct ggml_tensor*t=(const void*)(intptr_t)p;switch(k){case 0:return t->op;case 1:return (uint64_t)t->flags;case 2:return (uintptr_t)t->view_src;case 3:return (uint64_t)t->ne[i];case 4:return (uintptr_t)t->buffer;case 5:return (uintptr_t)t->src[i];case 6:return t->nb[i];case 7:return t->type;default:GGML_ABORT("Invalid shared tensor projection");}}
static inline int64_t fg160_i32(int64_t p,int64_t i){int32_t v;memcpy(&v,(const unsigned char*)(intptr_t)p+i*sizeof(v),sizeof(v));return v;}
static inline int64_t fg160_usage(int64_t p){return ggml_backend_buffer_get_usage((ggml_backend_buffer_t)(intptr_t)p);}
static inline uint64_t fg160_hash_find(int64_t p,int64_t t){const struct ggml_cgraph*g=(const void*)(intptr_t)p;return ggml_hash_find(&g->visited_hash_set,(const void*)(intptr_t)t);}
static inline int64_t fg160_hash_used(int64_t p,uint64_t i){const struct ggml_cgraph*g=(const void*)(intptr_t)p;return ggml_bitset_get(g->visited_hash_set.used,(size_t)i);}
static inline int64_t fg160_count(int64_t p,uint64_t i){return ((const struct ggml_cgraph*)(intptr_t)p)->use_counts[i];}
static inline uint64_t fg160_param_read(int64_t p,uint64_t offset){uint32_t v;memcpy(&v,(const unsigned char*)((const struct ggml_tensor*)(intptr_t)p)->op_params+offset,sizeof(v));return v;}
static inline void fg160_param_write(int64_t p,uint64_t offset,uint64_t value){uint32_t v=(uint32_t)value;memcpy((unsigned char*)((struct ggml_tensor*)(intptr_t)p)->op_params+offset,&v,sizeof(v));}
static inline void fg160_param_copy(int64_t p,int64_t data,uint64_t n){memcpy(((struct ggml_tensor*)(intptr_t)p)->op_params,(const void*)(intptr_t)data,(size_t)n);}
static inline int64_t fg160_glu(int64_t c,int64_t a,int64_t b,int64_t op){return (int64_t)(intptr_t)ggml_glu_impl((void*)(intptr_t)c,(void*)(intptr_t)a,(void*)(intptr_t)b,(enum ggml_glu_op)op,false);}
static inline int64_t fg160_argsort(int64_t c,int64_t a){return (int64_t)(intptr_t)ggml_argsort((void*)(intptr_t)c,(void*)(intptr_t)a,GGML_SORT_ORDER_DESC);}
static inline int64_t fg160_view(int64_t c,int64_t a,int64_t n0,int64_t n1,int64_t n2,int64_t n3,uint64_t b1,uint64_t b2,uint64_t b3){return (int64_t)(intptr_t)ggml_view_4d((void*)(intptr_t)c,(void*)(intptr_t)a,n0,n1,n2,n3,b1,b2,b3,0);}
static inline int64_t fg160_new(int64_t c,int64_t ty,int64_t n0,int64_t n1,int64_t n2,int64_t n3){return (int64_t)(intptr_t)ggml_new_tensor_4d((void*)(intptr_t)c,(enum ggml_type)ty,n0,n1,n2,n3);}
static inline void fg160_op_src(int64_t p,int64_t op,int64_t src){struct ggml_tensor*t=(void*)(intptr_t)p;t->op=(enum ggml_op)op;t->src[0]=(void*)(intptr_t)src;}
static inline void fg160_fail(int64_t code){GGML_ABORT("Goose shared fusion/parameter validation failed:code=%" PRId64,code);}
#define FG160_ENTRY fg160_gg_function_1
#define main fg160_unused_generated_main
#include "service-qualified.c"
#undef main
__attribute__((constructor)) static void fg160_config(void){const char*v=getenv("GGML_GOOSE_FUSION");fg160_fusion_enabled=!(v&&v[0]=='0'&&v[1]==0);v=getenv("GGML_GOOSE_GRAPH_LIFECYCLE");fg160_graph_enabled=!(v&&v[0]=='0'&&v[1]==0);}
uint64_t fg160_query(int64_t mode,const uint64_t*args,int64_t caller){if(caller<0||caller>=5||mode<0||mode>=64)GGML_ABORT("Invalid shared query ABI");uint64_t bit=UINT64_C(1)<<mode;if(!(fg160_thread_seen[caller]&bit)){fg160_thread_seen[caller]|=bit;atomic_fetch_or_explicit(&fg160_seen[caller],bit,memory_order_relaxed);}if(mode>=32){return fg159_header(mode-32,args[0],args[1],args[2]);}return FG160_ENTRY(mode,(int64_t)(intptr_t)args,1);}
__attribute__((visibility("default"))) uint64_t fg160_caller_seen(int caller);
uint64_t fg160_caller_seen(int caller){return caller>=0&&caller<5?atomic_load_explicit(&fg160_seen[caller],memory_order_relaxed):0;}
__attribute__((destructor)) static void fg160_report(void){static const char*roles[]={"core","cpu","cuda","scheduler","meta"};for(int i=0;i<5;++i){uint64_t m=fg160_caller_seen(i);if(m)fprintf(stderr,"FG160_SHARED caller=%s modes=0x%" PRIx64 "\n",roles[i],m);}}
#endif
