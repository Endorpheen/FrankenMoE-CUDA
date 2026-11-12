#include "bridge.h"
#include "ggml-cpu/simd-mappings.h"
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#ifndef FG_ELEMENT_TRACE
#define FG_ELEMENT_TRACE 0
#endif
static uint32_t fg_el_bits(float x){uint32_t u;memcpy(&u,&x,4);return u;}
static float fg_el_from_bits(uint32_t u){float x;memcpy(&x,&u,4);return x;}
static float fg_el_half(uint16_t x){return GGML_CPU_FP16_TO_FP32(x);}
static uint16_t fg_el_to_half(float x){return GGML_CPU_FP32_TO_FP16(x);}
#if FG_ELEMENT_TRACE
static atomic_uint_fast64_t fg_el_calls[2][23],fg_el_values[2][23];
#endif
static atomic_uint_fast64_t fg_el_vector_calls,fg_el_vector_values;
static void fg_element_vector_witness(int64_t values){
 atomic_fetch_add_explicit(&fg_el_vector_calls,1,memory_order_relaxed);
 atomic_fetch_add_explicit(&fg_el_vector_values,values,memory_order_relaxed);
}
static void fg_element_witness(int64_t mode,int64_t op,int64_t values){
#if FG_ELEMENT_TRACE
 if(mode<0||mode>1||op<0||op>=23)abort();
 atomic_fetch_add_explicit(&fg_el_calls[mode][op],1,memory_order_relaxed);
 atomic_fetch_add_explicit(&fg_el_values[mode][op],values,memory_order_relaxed);
#else
 (void)mode;(void)op;(void)values;
#endif
}
#define main fg_element_unused_main
#include "../../generated/elementwise-vector-trace.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg_el_plan),"Plan ABI");
_Static_assert(sizeof(gg_type_2)==sizeof(struct fg_el_shape),"Shape ABI");
_Static_assert(offsetof(gg_type_1,f3)==offsetof(struct fg_el_plan,a),"a ABI");
_Static_assert(offsetof(gg_type_1,f4)==offsetof(struct fg_el_plan,b),"b ABI");
_Static_assert(offsetof(gg_type_1,f5)==offsetof(struct fg_el_plan,d),"d ABI");
_Static_assert(offsetof(gg_type_1,f6)==offsetof(struct fg_el_plan,ith),"ith ABI");
_Static_assert(offsetof(gg_type_1,f7)==offsetof(struct fg_el_plan,nth),"nth ABI");
_Static_assert(offsetof(gg_type_1,f8)==offsetof(struct fg_el_plan,mode),"mode ABI");
_Static_assert(offsetof(gg_type_1,f9)==offsetof(struct fg_el_plan,op),"op ABI");
_Static_assert(offsetof(gg_type_1,f10)==offsetof(struct fg_el_plan,params),"params ABI");
void fg_element_compute(const struct fg_el_plan*p,const void*a,int64_t na,const void*b,int64_t nb,void*d,int64_t nd){
 gg_function_1((gg_type_1*)p,gg_view((void*)a,p->a.kind==0?na:0),gg_view((void*)a,p->a.kind!=0?na:0),gg_view((void*)b,p->b.kind==0?nb:0),gg_view((void*)b,p->b.kind!=0?nb:0),gg_view(d,p->d.kind==0?nd:0),gg_view(d,p->d.kind!=0?nd:0),gg_view((void*)a,p->a.kind==0?na/8:0),gg_view((void*)b,p->b.kind==0?nb/8:0),gg_view(d,p->d.kind==0?nd/8:0),0);
}
#if FG_ELEMENT_TRACE
__attribute__((destructor))static void fg_element_report(void){
 fprintf(stderr,"FG_SELFHOST_ELEMENT_VECTOR calls=%llu values=%llu\n",(unsigned long long)atomic_load(&fg_el_vector_calls),(unsigned long long)atomic_load(&fg_el_vector_values));
 uint64_t total=0;
 for(int mode=0;mode<2;++mode)for(int op=0;op<23;++op){uint64_t c=atomic_load(&fg_el_calls[mode][op]),n=atomic_load(&fg_el_values[mode][op]);total+=n;if(c)fprintf(stderr,"FG_SELFHOST_ELEMENT mode=%d op=%d calls=%llu values=%llu\n",mode,op,(unsigned long long)c,(unsigned long long)n);}
 fprintf(stderr,"FG_SELFHOST_ELEMENT total_values=%llu\n",(unsigned long long)total);
}
#endif
