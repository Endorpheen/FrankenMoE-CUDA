#ifndef FG157_CORE_H
#define FG157_CORE_H
#include <stdatomic.h>
#include <inttypes.h>
_Static_assert(sizeof(size_t)==8&&sizeof(double)==8,"64-bit core ABI");
static atomic_int_fast64_t fg157_live_context_heaps;
static atomic_uint_fast64_t fg157_os_counts[4];
static atomic_uint_fast64_t fg157_seen;
static _Thread_local uint64_t fg157_thread_seen;
static bool fg157_clock_seen;
#ifdef FG157_TESTING
static atomic_int fg157_fail_at;
static atomic_int fg157_attempt;
static bool fg157_injected_failure(void){int n=atomic_load(&fg157_fail_at);return n>0&&atomic_fetch_add(&fg157_attempt,1)+1==n;}
#else
static bool fg157_injected_failure(void){return false;}
#endif
static inline uint64_t fg157_arg(int64_t p,int64_t i){return ((struct fg157_args*)(intptr_t)p)->v[i];}
static inline int64_t fg157_slot(int64_t p,int64_t i){return (int64_t)(intptr_t)&((struct fg157_args*)(intptr_t)p)->v[i];}
static inline uint64_t fg157_load_u64(int64_t p){return *(const uint64_t*)(intptr_t)p;}
static inline uint64_t fg157_constant(int64_t i){switch(i){case 0:return GGML_MEM_ALIGN;case 1:return sizeof(struct ggml_context);case 3:return GGML_TYPE_COUNT;case 4:return GGML_OP_COUNT;case 5:return GGML_UNARY_OP_COUNT;case 6:return GGML_GLU_OP_COUNT;case 7:return GGML_OP_NONE;case 8:return GGML_OP_UNARY;case 9:return GGML_OP_GLU;case 10:return GGML_TYPE_F32;case 11:return GGML_MAX_NAME;case 12:return GGML_OBJECT_SIZE;case 13:return GGML_TENSOR_SIZE;case 14:return GGML_TENSOR_FLAG_INPUT;case 15:return GGML_TENSOR_FLAG_OUTPUT;case 16:return GGML_TENSOR_FLAG_PARAM;case 17:return GGML_TENSOR_FLAG_LOSS;case 18:return EINVAL;case 19:return ENOMEM;default:GGML_ABORT("Invalid Goose core constant");}}
static inline uint64_t fg157_read(int64_t p,int64_t k){if(k==7)return ((const struct ggml_object*)(intptr_t)p)->offs;if(k==8)return ((const struct ggml_object*)(intptr_t)p)->size;const struct ggml_context*c=(const void*)(intptr_t)p;switch(k){case 0:return c->mem_size;case 1:return (uintptr_t)c->mem_buffer;case 2:return c->mem_buffer_owned;case 3:return c->no_alloc;case 4:return c->n_objects;case 5:return (uintptr_t)c->objects_begin;case 6:return (uintptr_t)c->objects_end;default:GGML_ABORT("Invalid Goose context field");}}
static inline void fg157_write(int64_t p,int64_t k,uint64_t v){struct ggml_context*c=(void*)(intptr_t)p;switch(k){case 0:c->mem_size=v;return;case 1:c->mem_buffer=(void*)(uintptr_t)v;return;case 2:c->mem_buffer_owned=v!=0;return;case 3:c->no_alloc=v!=0;return;case 4:c->n_objects=(int)v;return;case 5:c->objects_begin=(void*)(uintptr_t)v;return;case 6:c->objects_end=(void*)(uintptr_t)v;return;default:GGML_ABORT("Invalid Goose context write");}}
static inline int64_t fg157_ctx_alloc(void){void*p=fg157_injected_failure()?NULL:malloc(sizeof(struct ggml_context));if(p)atomic_fetch_add(&fg157_live_context_heaps,1);return (int64_t)(intptr_t)p;}
static inline void fg157_ctx_release(int64_t p){if(p){atomic_fetch_sub(&fg157_live_context_heaps,1);free((void*)(intptr_t)p);}}
static inline void fg157_zero_ctx(int64_t p){memset((void*)(intptr_t)p,0,sizeof(struct ggml_context));}
static inline int64_t fg157_malloc(uint64_t n){return (int64_t)(intptr_t)(fg157_injected_failure()?NULL:malloc((size_t)n));}
static inline int64_t fg157_calloc(uint64_t n,uint64_t s){return (int64_t)(intptr_t)(fg157_injected_failure()?NULL:calloc((size_t)n,(size_t)s));}
static inline int64_t fg157_aligned_try(uint64_t n,int64_t out){void*p=NULL;int e=ENOMEM;atomic_fetch_add(&fg157_os_counts[0],1);
#if FG157_SUPPORTED_HOST
 if(!fg157_injected_failure())e=posix_memalign(&p,64,(size_t)n);
#endif
 *(uint64_t*)(intptr_t)out=(uint64_t)e;return (int64_t)(intptr_t)p;}
static inline void fg157_aligned_release(int64_t p){if(p)atomic_fetch_add(&fg157_os_counts[1],1);free((void*)(intptr_t)p);}
static inline void fg157_lock(void){ggml_critical_section_start();}
static inline void fg157_unlock(void){ggml_critical_section_end();}
static inline int64_t fg157_clock_ready(void){return fg157_clock_seen;}
static inline void fg157_clock_init(void){ggml_time_init();}
static inline void fg157_clock_mark(void){fg157_clock_seen=true;}
static inline void fg157_fail(int64_t code){GGML_ABORT("Goose context/core validation failed:code=%" PRId64,code);}
static inline void fg157_warn(int64_t code,uint64_t n){GGML_LOG_WARN("Goose allocation refused:code=%" PRId64 ",bytes=%" PRIu64 "\n",code,n);}
static inline uint64_t fg157_tget(int64_t p,int64_t k){const struct ggml_tensor*t=(const void*)(intptr_t)p;switch(k){case 0:return t->op;case 1:return (uint64_t)(int64_t)ggml_get_op_params_i32(t,0);case 2:return t->type;case 3:return (uint64_t)t->ne[0];case 4:return (uint64_t)t->ne[1];case 5:return (uint64_t)t->ne[2];case 6:return (uint64_t)t->ne[3];case 7:return t->flags;case 8:return (uintptr_t)t->view_src;case 9:return (uintptr_t)t->data;case 10:return (uintptr_t)t->name;default:GGML_ABORT("Invalid Goose tensor core field");}}
static inline void fg157_tflags(int64_t p,uint64_t f){((struct ggml_tensor*)(intptr_t)p)->flags=(int32_t)f;}
static inline uint64_t fg157_char_get(int64_t p,int64_t i){return ((const unsigned char*)(intptr_t)p)[i];}
static inline void fg157_char_put(int64_t p,int64_t i,uint64_t c){((unsigned char*)(intptr_t)p)[i]=(unsigned char)c;}
static inline void fg157_type_begin(int64_t i){memset(&fg157_goose_traits[i],0,sizeof(fg157_goose_traits[i]));fg157_goose_traits[i].type_name=fg157_type_names[i];fg157_goose_traits[i].to_float=fg157_native_traits[i].to_float;fg157_goose_traits[i].from_float_ref=fg157_native_traits[i].from_float_ref;}
static inline void fg157_type_put(int64_t i,int64_t k,uint64_t v){struct ggml_type_traits*t=&fg157_goose_traits[i];switch(k){case 0:t->blck_size=(int64_t)v;return;case 1:t->blck_size_interleave=(int64_t)v;return;case 2:t->type_size=v;return;case 3:t->is_quantized=v!=0;return;case 4:t->type_name=(const char*)(uintptr_t)v;return;default:GGML_ABORT("Invalid Goose type registry write");}}
static inline void fg157_type_byte(int64_t i,int64_t p,uint64_t v){fg157_type_names[i][p]=(char)v;}
static inline uint64_t fg157_type_get(int64_t i,int64_t k){const struct ggml_type_traits*t=&fg157_goose_traits[i];switch(k){case 0:return (uint64_t)t->blck_size;case 1:return (uint64_t)t->blck_size_interleave;case 2:return t->type_size;case 3:return t->is_quantized;case 4:return (uintptr_t)t->type_name;case 5:return (uintptr_t)t;default:GGML_ABORT("Invalid Goose type registry field");}}
static inline void fg157_label_begin(int64_t g,int64_t i){fg157_label_pointers[g][i]=fg157_label_bytes[g][i];}
static inline void fg157_label_byte(int64_t g,int64_t i,int64_t p,uint64_t v){fg157_label_bytes[g][i][p]=(char)v;}
static inline int64_t fg157_label_get(int64_t g,int64_t i){return (int64_t)(intptr_t)fg157_label_pointers[g][i];}
static inline uint64_t fg157_f64_bits(double v){uint64_t u;memcpy(&u,&v,sizeof(u));return u;}
#define FG157_ENTRY fg157_gg_function_1
#define main fg157_unused_generated_main
#include "context-qualified.c"
#undef main
static inline int64_t fg157_run(int64_t mode,struct fg157_args *args){uint64_t bit=UINT64_C(1)<<mode;if(!(fg157_thread_seen&bit)){fg157_thread_seen|=bit;atomic_fetch_or_explicit(&fg157_seen,bit,memory_order_relaxed);}return FG157_ENTRY(mode,(int64_t)(intptr_t)args,1);}
static int fg157_enabled=1;
__attribute__((constructor)) static void fg157_config(void){const char*v=getenv("GGML_GOOSE_CONTEXT_CORE");fg157_enabled=FG157_SUPPORTED_HOST&&!(v&&v[0]=='0'&&v[1]==0);if(fg157_enabled){struct fg157_args a={0};fg157_run(26,&a);}}
__attribute__((visibility("default"))) uint64_t fg157_core_seen(void);
uint64_t fg157_core_seen(void){return atomic_load(&fg157_seen);}
__attribute__((visibility("default"))) int64_t fg157_context_heap_live(void);
int64_t fg157_context_heap_live(void){return atomic_load(&fg157_live_context_heaps);}
__attribute__((visibility("default"))) uint64_t fg157_aligned_free_calls(void);
uint64_t fg157_aligned_free_calls(void){return atomic_load(&fg157_os_counts[1]);}
#ifdef FG157_TESTING
__attribute__((visibility("default"))) void fg157_fail_allocation_at(int n);
void fg157_fail_allocation_at(int n){atomic_store(&fg157_attempt,0);atomic_store(&fg157_fail_at,n);}
__attribute__((visibility("default"))) void *fg157_test_private_alloc(size_t n,size_t size,int calloc_mode);
void *fg157_test_private_alloc(size_t n,size_t size,int calloc_mode){return calloc_mode?ggml_calloc(n,size):ggml_malloc(n);}
#endif
__attribute__((destructor)) static void fg157_report(void){uint64_t m=fg157_core_seen();if(m)fprintf(stderr,"FG157_CONTEXT_CORE goose_modes=0x%" PRIx64 ",live_context_heaps=%" PRId64 "\n",m,fg157_context_heap_live());}
#endif
