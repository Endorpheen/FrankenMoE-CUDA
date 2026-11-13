#define GGML_COMMON_DECL_C
#define GGML_COMMON_IMPL_C
#include "ggml-common.h"
#include "bridge.h"
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
static uint32_t fg143_bits(float x){uint32_t v;memcpy(&v,&x,4);return v;}
static float fg143_from_bits(uint32_t v){float x;memcpy(&x,&v,4);return x;}
static float fg143_half(uint16_t v){return _cvtsh_ss(v);}
static float fg141_half(uint16_t v){return _cvtsh_ss(v);}
static float fg143_codec_from_bits(uint32_t v){float x;memcpy(&x,&v,4);return x;}
static void fg141_witness(int64_t s,int64_t k,int64_t n){(void)s;(void)k;(void)n;}
static uint16_t fg143_to_half(float v){return _cvtss_sh(v,_MM_FROUND_TO_NEAREST_INT);}
#ifndef FG143_TRACE
#define FG143_TRACE 1
#endif
#if FG143_TRACE
static atomic_uint_fast64_t fg143_calls[128],fg143_values[128];
#endif
static void fg143_witness(int64_t op,int64_t values){
#if FG143_TRACE
 if(op<0||op>=128||values<0)__builtin_trap();atomic_fetch_add_explicit(fg143_calls+op,1,memory_order_relaxed);atomic_fetch_add_explicit(fg143_values+op,values,memory_order_relaxed);
#else
 (void)op;(void)values;
#endif
}
#define main fg143_unused_main
#include "cpu-backend.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg143_node),"Node ABI");
_Static_assert(sizeof(gg_type_2)==sizeof(struct fg143_tensor),"Tensor ABI");
_Static_assert(offsetof(gg_type_1,f4)==offsetof(struct fg143_node,x),"x ABI");
_Static_assert(offsetof(gg_type_1,f5)==offsetof(struct fg143_node,y),"y ABI");
_Static_assert(offsetof(gg_type_1,f6)==offsetof(struct fg143_node,z),"z ABI");
_Static_assert(offsetof(gg_type_1,f7)==offsetof(struct fg143_node,d),"d ABI");
_Static_assert(offsetof(gg_type_1,f8)==offsetof(struct fg143_node,op),"op ABI");
_Static_assert(offsetof(gg_type_1,f9)==offsetof(struct fg143_node,ith),"ith ABI");
_Static_assert(offsetof(gg_type_1,f10)==offsetof(struct fg143_node,nth),"nth ABI");
_Static_assert(offsetof(gg_type_1,f11)==offsetof(struct fg143_node,ip),"ip ABI");
_Static_assert(offsetof(gg_type_1,f12)==offsetof(struct fg143_node,fp),"fp ABI");
_Static_assert(offsetof(gg_type_2,f0)==offsetof(struct fg143_tensor,ne),"ne tensor ABI");
_Static_assert(offsetof(gg_type_2,f1)==offsetof(struct fg143_tensor,nb),"nb tensor ABI");
_Static_assert(offsetof(gg_type_2,f2)==offsetof(struct fg143_tensor,kind),"kind tensor ABI");
_Static_assert(offsetof(gg_type_2,f3)==offsetof(struct fg143_tensor,bytes),"bytes tensor ABI");
void fg143_workspace(const struct fg143_node*p,int64_t*out){struct fg143_node local=*p;local.ip[15]=1464816203;uint8_t ok=gg_function_1((gg_type_1*)&local,gg_view(0,0),gg_view(0,0),gg_view(0,0),gg_view(0,0),gg_view(0,0),gg_view(out,4),gg_view((void*)iq2xxs_grid,256),gg_view((void*)iq2xs_grid,512),gg_view((void*)iq2s_grid,1024),gg_view((void*)iq3xxs_grid,256),gg_view((void*)iq3s_grid,512),gg_view((void*)iq1s_grid,NGRID_IQ1S),gg_view((void*)ksigns_iq2xs,128),gg_view((void*)kvalues_iq4nl,16),0);if(!ok)__builtin_trap();}
uint8_t fg143_compute(const struct fg143_node*p,const void*x,const void*y,const void*z,void*dst,float*scratch,int64_t nf,int64_t*indices,int64_t ni){return gg_function_1((gg_type_1*)p,gg_view((void*)x,p->x.bytes),gg_view((void*)y,p->y.bytes),gg_view((void*)z,p->z.bytes),gg_view(dst,p->d.bytes),gg_view(scratch,nf),gg_view(indices,ni),gg_view((void*)iq2xxs_grid,256),gg_view((void*)iq2xs_grid,512),gg_view((void*)iq2s_grid,1024),gg_view((void*)iq3xxs_grid,256),gg_view((void*)iq3s_grid,512),gg_view((void*)iq1s_grid,NGRID_IQ1S),gg_view((void*)ksigns_iq2xs,128),gg_view((void*)kvalues_iq4nl,16),0);}
#if FG143_TRACE
__attribute__((destructor))static void fg143_report(void){for(int i=0;i<128;++i){uint64_t n=atomic_load(fg143_calls+i);if(n)fprintf(stderr,"FG143_CPU op=%d calls=%llu values=%llu\n",i,(unsigned long long)n,(unsigned long long)atomic_load(fg143_values+i));}}
#endif
