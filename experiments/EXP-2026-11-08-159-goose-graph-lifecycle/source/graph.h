#ifndef FG159_GRAPH_H
#define FG159_GRAPH_H
#include <stdatomic.h>
#include <inttypes.h>
static atomic_int_fast64_t fg159_heap_count;
static atomic_uint_fast64_t fg159_seen;
static _Thread_local uint64_t fg159_thread_seen;
#ifdef FG159_TESTING
static atomic_int fg159_fail_at;
static atomic_int fg159_attempt;
static bool fg159_injected_failure(void){int n=atomic_load(&fg159_fail_at);return n>0&&atomic_fetch_add(&fg159_attempt,1)+1==n;}
#else
static bool fg159_injected_failure(void){return false;}
#endif
static inline uint64_t fg159_arg(int64_t p,int64_t i){return ((const uint64_t*)(intptr_t)p)[i];}
static inline uint64_t fg159_constant(int64_t k){switch(k){case 0:return sizeof(struct ggml_cgraph);case 1:return GGML_MEM_ALIGN;case 2:return GGML_OBJECT_SIZE;case 5:return GGML_MAX_SRC;case 6:return GGML_TENSOR_FLAG_PARAM;case 7:return GGML_TENSOR_FLAG_COMPUTE;case 8:return GGML_OP_NONE;default:GGML_ABORT("Invalid graph constant");}}
static inline uint64_t fg159_graph(int64_t p,int64_t k){const struct ggml_cgraph*g=(const void*)(intptr_t)p;switch(k){case 0:return (uint64_t)(int64_t)g->size;case 1:return (uint64_t)(int64_t)g->n_nodes;case 2:return (uint64_t)(int64_t)g->n_leafs;case 3:return (uintptr_t)g->nodes;case 4:return (uintptr_t)g->leafs;case 5:return (uintptr_t)g->use_counts;case 6:return (uintptr_t)g->grads;case 7:return (uintptr_t)g->grad_accs;case 8:return g->order;case 9:return g->uid;case 10:return (uintptr_t)&g->visited_hash_set;default:GGML_ABORT("Invalid graph projection");}}
static inline void fg159_graph_put(int64_t p,int64_t k,uint64_t v){struct ggml_cgraph*g=(void*)(intptr_t)p;switch(k){case 0:g->size=(int)v;return;case 1:g->n_nodes=(int)v;return;case 2:g->n_leafs=(int)v;return;case 3:g->nodes=(void*)(uintptr_t)v;return;case 4:g->leafs=(void*)(uintptr_t)v;return;case 5:g->use_counts=(void*)(uintptr_t)v;return;case 6:g->grads=(void*)(uintptr_t)v;return;case 7:g->grad_accs=(void*)(uintptr_t)v;return;case 8:g->order=(enum ggml_cgraph_eval_order)v;return;case 9:g->uid=v;return;default:GGML_ABORT("Invalid graph write");}}
static inline uint64_t fg159_hash(int64_t p,int64_t k){const struct ggml_hash_set*h=(const void*)(intptr_t)p;switch(k){case 0:return h->size;case 1:return (uintptr_t)h->used;case 2:return (uintptr_t)h->keys;default:GGML_ABORT("Invalid hash projection");}}
static inline void fg159_hash_put(int64_t p,int64_t k,uint64_t v){struct ggml_hash_set*h=(void*)(intptr_t)p;switch(k){case 0:h->size=v;return;case 1:h->used=(void*)(uintptr_t)v;return;case 2:h->keys=(void*)(uintptr_t)v;return;default:GGML_ABORT("Invalid hash write");}}
_Static_assert(sizeof(uintptr_t)==8&&sizeof(struct ggml_tensor*)==8,"64-bit graph storage ABI");
// The same primitive accesses pointer arrays and numeric DFS frames without aliasing them.
static inline uint64_t fg159_ptr(int64_t p,uint64_t i){uintptr_t v;memcpy(&v,(const unsigned char*)(intptr_t)p+i*sizeof(v),sizeof(v));return v;}
static inline void fg159_ptr_put(int64_t p,uint64_t i,uint64_t value){uintptr_t v=(uintptr_t)value;memcpy((unsigned char*)(intptr_t)p+i*sizeof(v),&v,sizeof(v));}
static inline uint64_t fg159_u32(int64_t p,uint64_t i){return ((const uint32_t*)(intptr_t)p)[i];}
static inline void fg159_u32_put(int64_t p,uint64_t i,uint64_t v){((uint32_t*)(intptr_t)p)[i]=(uint32_t)v;}
static inline void fg159_zero(int64_t p,uint64_t n){memset((void*)(intptr_t)p,0,(size_t)n);}
static inline int64_t fg159_alloc(uint64_t n,int64_t clear){void*p=fg159_injected_failure()?NULL:(clear?calloc(1,(size_t)n):malloc((size_t)n));if(p)atomic_fetch_add(&fg159_heap_count,1);return (int64_t)(intptr_t)p;}
static inline void fg159_free(int64_t p){if(p){atomic_fetch_sub(&fg159_heap_count,1);free((void*)(intptr_t)p);}}
static inline int64_t fg159_arena(int64_t p,uint64_t n){struct ggml_context*c=(void*)(intptr_t)p;struct ggml_object*o=ggml_new_object(c,GGML_OBJECT_TYPE_GRAPH,(size_t)n);return (int64_t)(intptr_t)((char*)c->mem_buffer+o->offs);}
static inline uint64_t fg159_tensor(int64_t p,int64_t k,int64_t i){const struct ggml_tensor*t=(const void*)(intptr_t)p;switch(k){case 0:return t->op;case 1:return (uint64_t)t->flags;case 2:return (uintptr_t)t->src[i];case 3:return (unsigned char)t->name[0];default:GGML_ABORT("Invalid traversal tensor projection");}}
static inline void fg159_flags(int64_t p,uint64_t v){((struct ggml_tensor*)(intptr_t)p)->flags=(int32_t)v;}
static inline void fg159_name(int64_t p,int64_t kind,uint64_t i){ggml_format_name((void*)(intptr_t)p,kind==0?"leaf_%d":"node_%d",(int)i);}
static inline int64_t fg159_equal_name(int64_t p,int64_t s){return strcmp(((const struct ggml_tensor*)(intptr_t)p)->name,(const char*)(intptr_t)s)==0;}
static inline void fg159_fail(int64_t code){GGML_ABORT("Goose graph lifecycle validation failed:code=%" PRId64,code);}
#define FG159_ENTRY fg159_gg_function_1
#define main fg159_unused_generated_main
#include "graph-qualified.c"
#undef main
static int fg159_enabled=1;
__attribute__((constructor)) static void fg159_config(void){const char*v=getenv("GGML_GOOSE_GRAPH_LIFECYCLE");fg159_enabled=!(v&&v[0]=='0'&&v[1]==0);}
static inline uint64_t fg159_run(const uint64_t*a){uint64_t bit=UINT64_C(1)<<a[0];if(!(fg159_thread_seen&bit)){fg159_thread_seen|=bit;atomic_fetch_or_explicit(&fg159_seen,bit,memory_order_relaxed);}return FG159_ENTRY((int64_t)(intptr_t)a,1);}
#define FG159_ARG(x) ((uint64_t)(uintptr_t)(x))
#define FG159_CALL(...) fg159_run((const uint64_t[]){__VA_ARGS__})
static inline uint64_t fg159_header(int64_t mode,uint64_t a,uint64_t b,uint64_t c){return FG159_CALL((uint64_t)mode,a,b,c);}
__attribute__((visibility("default"))) uint64_t fg159_graph_seen(void);
uint64_t fg159_graph_seen(void){return atomic_load_explicit(&fg159_seen,memory_order_relaxed);}
__attribute__((visibility("default"))) int64_t fg159_graph_heap_live(void);
int64_t fg159_graph_heap_live(void){return atomic_load(&fg159_heap_count);}
#ifdef FG159_TESTING
__attribute__((visibility("default"))) void fg159_fail_allocation_at(int n);
void fg159_fail_allocation_at(int n){atomic_store(&fg159_attempt,0);atomic_store(&fg159_fail_at,n);}
__attribute__((visibility("default"))) uint64_t fg159_test_hash(int mode,void*p,void*k,uint64_t index);
uint64_t fg159_test_hash(int mode,void*p,void*k,uint64_t index){return fg159_header(mode,(uintptr_t)p,k?(uintptr_t)k:index,0);}
#endif
__attribute__((destructor)) static void fg159_report(void){uint64_t m=fg159_graph_seen();if(m)fprintf(stderr,"FG159_GRAPH_LIFECYCLE goose_modes=0x%" PRIx64 ",live_graph_heaps=%" PRId64 "\n",m,fg159_graph_heap_live());}
#endif
