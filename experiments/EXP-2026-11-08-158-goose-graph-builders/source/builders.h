#ifndef FG158_BUILDERS_H
#define FG158_BUILDERS_H
#include <stdatomic.h>
#include <inttypes.h>
static inline bool ggml_can_mul_mat(const struct ggml_tensor*,const struct ggml_tensor*);
static inline bool ggml_can_out_prod(const struct ggml_tensor*,const struct ggml_tensor*);
static inline int64_t fg158_arg(int64_t p,int64_t i){return (int64_t)((const uint64_t*)(intptr_t)p)[i];}
static inline int64_t fg158_ne(int64_t t,int64_t i){return ((const struct ggml_tensor*)(intptr_t)t)->ne[i];}
static inline int64_t fg158_prop(int64_t t,int64_t k){const struct ggml_tensor*a=(const void*)(intptr_t)t;switch(k){case 0:return a->type;case 1:return ggml_is_quantized(a->type);case 2:return a->op;case 3:return a->name[0]!=0;default:GGML_ABORT("Invalid builder field");}}
static inline int64_t fg158_src(int64_t t,int64_t i){return (int64_t)(intptr_t)((const struct ggml_tensor*)(intptr_t)t)->src[i];}
static inline int64_t fg158_param(int64_t t,int64_t i){return ggml_get_op_params_i32((const void*)(intptr_t)t,i);}
static inline void fg158_set_op(int64_t t,int64_t op){((struct ggml_tensor*)(intptr_t)t)->op=(enum ggml_op)op;}
static inline void fg158_set_src(int64_t t,int64_t i,int64_t v){((struct ggml_tensor*)(intptr_t)t)->src[i]=(void*)(intptr_t)v;}
static inline void fg158_set_param(int64_t t,int64_t i,int64_t v){ggml_set_op_params_i32((void*)(intptr_t)t,(int)i,(int32_t)v);}
static inline int64_t fg158_new(int64_t c,int64_t ty,int64_t n0,int64_t n1,int64_t n2,int64_t n3){int64_t ne[]={n0,n1,n2,n3};return (int64_t)(intptr_t)ggml_new_tensor((void*)(intptr_t)c,(enum ggml_type)ty,4,ne);}
static inline int64_t fg158_dup(int64_t c,int64_t a){return (int64_t)(intptr_t)ggml_dup_tensor((void*)(intptr_t)c,(const void*)(intptr_t)a);}
static inline int64_t fg158_view(int64_t c,int64_t a){return (int64_t)(intptr_t)ggml_view_tensor((void*)(intptr_t)c,(void*)(intptr_t)a);}
static inline int64_t fg158_pred(int64_t which,int64_t a,int64_t b){const struct ggml_tensor*x=(const void*)(intptr_t)a,*y=(const void*)(intptr_t)b;switch(which){case 0:return ggml_is_contiguous(x);case 1:return ggml_is_contiguous_rows(x);case 2:return ggml_is_padded_1d(x);case 3:return ggml_are_same_shape(x,y);case 4:return ggml_can_repeat(x,y);case 5:return ggml_is_scalar(x);case 6:return ggml_can_repeat_rows(x,y);case 7:return ggml_is_contiguous_1(x);case 8:return ggml_can_mul_mat(x,y);case 9:return ggml_can_out_prod(x,y);case 10:return ggml_is_transposed(x);case 11:return ggml_is_matrix(x);case 12:return ggml_is_empty(x);case 13:return ggml_nelements(x);case 14:return ggml_is_vector(x);default:GGML_ABORT("Invalid builder predicate");}}
static inline int64_t fg158_i32(int64_t p,int64_t i){return ((const int32_t*)(intptr_t)p)[i];}
static inline void fg158_name(int64_t t,int64_t a,int64_t b,int64_t style){struct ggml_tensor*r=(void*)(intptr_t)t;const struct ggml_tensor*x=(const void*)(intptr_t)a,*y=(const void*)(intptr_t)b;if(style==0){ggml_format_name(r,"%s (copy of %s)",y->name,x->name);}else ggml_format_name(r,style==1?"%s (copy)":"%s (cont)",x->name);}
static inline float fg158_float(int64_t x){uint32_t bits=(uint32_t)x;float f;memcpy(&f,&bits,sizeof f);return f;}
static inline uint64_t fg158_bits(float f){uint32_t bits;memcpy(&bits,&f,sizeof bits);return bits;}
static inline void fg158_fail(int64_t code){GGML_ABORT("Goose graph builder validation failed:code=%" PRId64,code);}
static int fg158_enabled=1;
static atomic_uint_fast64_t fg158_seen;
static _Thread_local uint64_t fg158_thread_seen;
__attribute__((constructor)) static void fg158_config(void){const char*v=getenv("GGML_GOOSE_GRAPH_BUILDERS");fg158_enabled=!(v&&v[0]=='0'&&v[1]==0);}
#define FG158_ENTRY fg158_gg_function_1
#define main fg158_unused_generated_main
#include "builders-qualified.c"
#undef main
static inline int64_t fg158_run(const uint64_t *args){int mode=(int)args[0];uint64_t bit=UINT64_C(1)<<mode;if(!(fg158_thread_seen&bit)){fg158_thread_seen|=bit;atomic_fetch_or_explicit(&fg158_seen,bit,memory_order_relaxed);}return FG158_ENTRY((int64_t)(intptr_t)args,1);}
#define FG158_ARG(x) ((uint64_t)(uintptr_t)(x))
#define FG158_CALL(mode,c,a,b,d,op,inplace,...) fg158_run((const uint64_t[]){FG158_ARG(mode),FG158_ARG(c),FG158_ARG(a),FG158_ARG(b),FG158_ARG(d),FG158_ARG(op),FG158_ARG(inplace),__VA_ARGS__})
__attribute__((visibility("default"))) uint64_t fg158_builder_seen(void);
uint64_t fg158_builder_seen(void){return atomic_load_explicit(&fg158_seen,memory_order_relaxed);}
__attribute__((destructor)) static void fg158_report(void){uint64_t m=fg158_builder_seen();if(m)fprintf(stderr,"FG158_GRAPH_BUILDERS goose_modes=0x%" PRIx64 "\n",m);}
#endif
