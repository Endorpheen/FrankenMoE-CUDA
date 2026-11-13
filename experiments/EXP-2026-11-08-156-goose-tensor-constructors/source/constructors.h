#ifndef FG156_CONSTRUCTORS_H
#define FG156_CONSTRUCTORS_H
#include <stdatomic.h>
#include <inttypes.h>
_Static_assert(sizeof(uintptr_t)==sizeof(uint64_t),"64-bit constructor ABI");
_Static_assert(GGML_MAX_DIMS==4,"Four-dimensional constructor ABI");
_Static_assert(GGML_OP_NONE==0,"Zeroed tensor default operation");
static inline uint64_t fg156_arg(int64_t p,int64_t i){return ((const uint64_t *)(intptr_t)p)[i];}
static inline int64_t fg156_dim(int64_t p,int64_t i){return ((const int64_t *)(intptr_t)p)[i];}
static inline uint64_t fg156_constant(int64_t i){switch(i){case 0:return GGML_MEM_ALIGN;case 1:return GGML_OBJECT_SIZE;case 2:return GGML_TENSOR_SIZE;case 3:return GGML_TYPE_COUNT;case 4:return GGML_OBJECT_TYPE_TENSOR;case 5:return sizeof(struct ggml_tensor);case 6:return GGML_OP_RESHAPE;case 7:return GGML_OP_VIEW;case 8:return GGML_OP_PERMUTE;case 9:return GGML_OP_TRANSPOSE;case 10:return GGML_OBJECT_TYPE_WORK_BUFFER;default:GGML_ABORT("Invalid constructor constant");}}
static inline uint64_t fg156_ctx(int64_t p,int64_t i){const struct ggml_context *c=(const void *)(intptr_t)p;switch(i){case 0:return c->mem_size;case 1:return (uintptr_t)c->mem_buffer;case 2:return (uintptr_t)c->objects_begin;case 3:return (uintptr_t)c->objects_end;case 4:return c->no_alloc;case 5:return c->n_objects;default:GGML_ABORT("Invalid context projection");}}
static inline void fg156_ctx_set(int64_t p,int64_t i,uint64_t v){struct ggml_context*c=(void *)(intptr_t)p;switch(i){case 2:c->objects_begin=(void *)(uintptr_t)v;return;case 3:c->objects_end=(void *)(uintptr_t)v;return;case 5:c->n_objects=(int)v;return;default:GGML_ABORT("Invalid context projection write");}}
static inline uint64_t fg156_obj(int64_t p,int64_t i){const struct ggml_object*o=(const void *)(intptr_t)p;switch(i){case 0:return o->offs;case 1:return o->size;case 2:return (uintptr_t)o->next;case 3:return o->type;default:GGML_ABORT("Invalid object projection");}}
static inline void fg156_obj_set(int64_t p,int64_t i,uint64_t v){struct ggml_object*o=(void *)(intptr_t)p;switch(i){case 0:o->offs=v;return;case 1:o->size=v;return;case 2:o->next=(void *)(uintptr_t)v;return;case 3:o->type=(enum ggml_object_type)v;return;default:GGML_ABORT("Invalid object projection write");}}
static inline int64_t fg156_ptr_add(int64_t p,uint64_t n){return (int64_t)((uintptr_t)p+(uintptr_t)n);}
static inline void fg156_zero(int64_t p,uint64_t n){memset((void *)(intptr_t)p,0,(size_t)n);}
static inline uint64_t fg156_tget(int64_t p,int64_t k,int64_t i){const struct ggml_tensor*t=(const void *)(intptr_t)p;switch(k){case 0:return (uint64_t)t->ne[i];case 1:return t->nb[i];case 2:return (uintptr_t)t->view_src;case 3:return t->view_offs;case 4:return (uintptr_t)t->data;case 5:return t->type;case 6:return (uintptr_t)t->ne;default:GGML_ABORT("Invalid tensor projection");}}
static inline void fg156_tset(int64_t p,int64_t k,int64_t i,uint64_t v){struct ggml_tensor*t=(void *)(intptr_t)p;switch(k){case 0:t->ne[i]=(int64_t)v;return;case 1:t->nb[i]=v;return;case 2:t->view_src=(void *)(uintptr_t)v;return;case 3:t->view_offs=v;return;case 4:t->data=(void *)(uintptr_t)v;return;case 5:t->type=(enum ggml_type)v;return;case 7:t->op=(enum ggml_op)v;return;case 8:t->src[i]=(void *)(uintptr_t)v;return;case 9:memcpy(t->op_params,&v,sizeof(v));return;case 10:{int32_t q=(int32_t)v;memcpy((char *)t->op_params+i*sizeof(q),&q,sizeof(q));return;}default:GGML_ABORT("Invalid tensor projection write");}}
static inline int64_t fg156_type_block(int64_t t){return ggml_blck_size((enum ggml_type)t);}
static inline uint64_t fg156_type_size(int64_t t){return ggml_type_size((enum ggml_type)t);}
static inline uint64_t fg156_span(int64_t t){return ggml_nbytes((const void *)(intptr_t)t);}
static inline int64_t fg156_elements(int64_t t){return ggml_nelements((const void *)(intptr_t)t);}
static inline int64_t fg156_contiguous(int64_t t){return ggml_is_contiguous((const void *)(intptr_t)t);}
static inline void fg156_name(int64_t t,int64_t src,int64_t style){const struct ggml_tensor*s=(const void *)(intptr_t)src;const char *fmt;switch(style){case 0:fmt="%s (view)";break;case 1:fmt="%s (reshaped)";break;case 2:fmt="%s (permuted)";break;case 3:fmt="%s (transposed)";break;default:GGML_ABORT("Invalid constructor name style");}ggml_format_name((void *)(intptr_t)t,fmt,s->name);}
static inline int64_t fg156_name_equal(int64_t t,int64_t s){return strcmp(((const struct ggml_tensor *)(intptr_t)t)->name,(const char *)(intptr_t)s)==0;}
static inline void fg156_store_index(int64_t p,int64_t v){if(p)*(int64_t *)(intptr_t)p=v;}
static inline void fg156_warn_arena(int64_t code,uint64_t need,uint64_t cap){GGML_LOG_WARN("Goose arena allocation refused:code=%" PRId64 ",needed=%" PRIu64 ",capacity=%" PRIu64 "\n",code,need,cap);}
static inline void fg156_fail(int64_t code){GGML_ABORT("Goose tensor constructor validation failed:code=%" PRId64,code);}
static int fg156_enabled=1;
static atomic_uint_fast64_t fg156_seen;
static _Thread_local uint64_t fg156_thread_seen;
__attribute__((constructor)) static void fg156_config(void){const char*v=getenv("GGML_GOOSE_TENSOR_CONSTRUCTORS");fg156_enabled=!(v&&v[0]=='0'&&v[1]==0);}
#define FG156_ENTRY fg156_gg_function_1
#define main fg156_unused_generated_main
#include "constructors-qualified.c"
#undef main
static inline int64_t fg156_run(int64_t mode,const uint64_t *args){uint64_t bit=UINT64_C(1)<<mode;if(!(fg156_thread_seen&bit)){fg156_thread_seen|=bit;atomic_fetch_or_explicit(&fg156_seen,bit,memory_order_relaxed);}return FG156_ENTRY(mode,(int64_t)(intptr_t)args,1);}
#define FG156_CALL(mode,...) fg156_run((mode),(const uint64_t[]){__VA_ARGS__})
#define FG156_ARG(x) ((uint64_t)(uintptr_t)(x))
__attribute__((visibility("default"))) uint64_t fg156_constructor_seen(void);
uint64_t fg156_constructor_seen(void){return atomic_load_explicit(&fg156_seen,memory_order_relaxed);}
__attribute__((visibility("default"))) uint64_t fg156_context_count(const struct ggml_context *ctx);
uint64_t fg156_context_count(const struct ggml_context *ctx){return (uint64_t)ctx->n_objects;}
__attribute__((destructor)) static void fg156_report(void){uint64_t m=fg156_constructor_seen();if(m)fprintf(stderr,"FG156_CONSTRUCTORS goose_modes=0x%" PRIx64 "\n",m);}
#endif
