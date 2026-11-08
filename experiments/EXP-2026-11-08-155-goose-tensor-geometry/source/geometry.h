#ifndef FG155_GEOMETRY_H
#define FG155_GEOMETRY_H
#include <stdatomic.h>
#include <stdlib.h>
#include <inttypes.h>
static inline int64_t fg155_ne(int64_t p,int64_t i){return ((const struct ggml_tensor *)(intptr_t)p)->ne[i];}
static inline uint64_t fg155_nb(int64_t p,int64_t i){return ((const struct ggml_tensor *)(intptr_t)p)->nb[i];}
static inline int64_t fg155_tensor_block(int64_t p){return ggml_blck_size(((const struct ggml_tensor *)(intptr_t)p)->type);}
static inline uint64_t fg155_tensor_bytes(int64_t p){return ggml_type_size(((const struct ggml_tensor *)(intptr_t)p)->type);}
static inline int64_t fg155_type_block(int64_t t){return ggml_blck_size((enum ggml_type)t);}
static inline uint64_t fg155_type_bytes(int64_t t){return ggml_type_size((enum ggml_type)t);}
static inline uint64_t fg155_alignment(void){return GGML_MEM_ALIGN;}
static inline void fg155_fail(void){GGML_ABORT("Invalid Goose tensor geometry");}
static int fg155_enabled=1;
static atomic_uint_fast64_t fg155_seen;
static _Thread_local uint64_t fg155_thread_seen;
__attribute__((constructor)) static void fg155_config(void){const char*p=getenv("GGML_GOOSE_TENSOR_GEOMETRY");fg155_enabled=!(p&&p[0]=='0'&&p[1]==0);}
#define main fg155_unused_generated_main
#include "geometry-generated.c"
#undef main
static inline int64_t fg155_geometry(int64_t mode,int64_t a,int64_t b,int64_t c){
 uint64_t bit=UINT64_C(1)<<mode;
 if(!(fg155_thread_seen&bit)){fg155_thread_seen|=bit;atomic_fetch_or_explicit(&fg155_seen,bit,memory_order_relaxed);}
 return gg_function_1(mode,a,b,c,1);
}
__attribute__((visibility("default"))) uint64_t fg155_geometry_seen(void);
uint64_t fg155_geometry_seen(void){return atomic_load_explicit(&fg155_seen,memory_order_relaxed);}
__attribute__((destructor)) static void fg155_report(void){uint64_t m=fg155_geometry_seen();if(m)fprintf(stderr,"FG155_GEOMETRY goose_modes=0x%" PRIx64 "\n",m);}
#endif
