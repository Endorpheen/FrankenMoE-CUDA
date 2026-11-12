#include "bridge.h"
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>
static uint32_t fg143_bits(float x){uint32_t u;memcpy(&u,&x,4);return u;}
static float fg143_from_bits(uint32_t u){float x;memcpy(&x,&u,4);return x;}
static float fg143_half(uint16_t u){return _cvtsh_ss(u);}
static uint16_t fg143_to_half(float x){return _cvtss_sh(x,_MM_FROUND_TO_NEAREST_INT);}
static atomic_uint_fast64_t stateful_calls[128];
static void fg143_witness(int64_t op,int64_t values){(void)values;if(op<0||op>=128)__builtin_trap();atomic_fetch_add_explicit(stateful_calls+op,1,memory_order_relaxed);}
#define main fg143_state_unused_main
#include "../../../generated/cpu-stateful.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg143_state_plan),"StatePlan ABI");
_Static_assert(sizeof(gg_type_2)==sizeof(struct fg143_tensor),"Tensor ABI");
_Static_assert(offsetof(gg_type_1,f4)==offsetof(struct fg143_state_plan,source),"source ABI");
_Static_assert(offsetof(gg_type_1,f5)==offsetof(struct fg143_state_plan,dst),"dst ABI");
_Static_assert(offsetof(gg_type_1,f6)==offsetof(struct fg143_state_plan,ith),"ith ABI");
_Static_assert(offsetof(gg_type_1,f7)==offsetof(struct fg143_state_plan,nth),"nth ABI");
_Static_assert(offsetof(gg_type_1,f8)==offsetof(struct fg143_state_plan,op),"op ABI");
_Static_assert(offsetof(gg_type_1,f9)==offsetof(struct fg143_state_plan,snapshots),"snapshots ABI");
_Static_assert(offsetof(gg_type_1,f10)==offsetof(struct fg143_state_plan,fp),"fp ABI");
uint8_t fg143_state_compute(const struct fg143_state_plan*p,const void*const*inputs,void*dst){return gg_function_1((gg_type_1*)p,gg_view((void*)inputs[0],p->source[0].bytes),gg_view((void*)inputs[1],p->source[1].bytes),gg_view((void*)inputs[2],p->source[2].bytes),gg_view((void*)inputs[3],p->source[3].bytes),gg_view((void*)inputs[4],p->source[4].bytes),gg_view((void*)inputs[5],p->source[5].bytes),gg_view((void*)inputs[6],p->source[6].bytes),gg_view(dst,p->dst.bytes),0);}
__attribute__((destructor))static void fg143_state_report(void){for(int i=0;i<128;++i){uint64_t n=atomic_load(stateful_calls+i);if(n)fprintf(stderr,"FG143_STATE op=%d calls=%llu\n",i,(unsigned long long)n);}}
