#define GGML_COMMON_DECL_C
#define GGML_COMMON_IMPL_C
#include "ggml-common.h"
#include "ggml-quants.h"
#include <immintrin.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>
static float fg141_half(uint16_t x){return _cvtsh_ss(x);}
static float fg143_codec_from_bits(uint32_t x){float f;memcpy(&f,&x,4);return f;}
static atomic_uint_fast64_t codec_calls[43];
static void fg141_witness(int64_t s,int64_t k,int64_t n){(void)s;(void)k;(void)n;}
#define main fg143_codec_unused_main
#include "../../../generated/cpu-backend-codec.c"
#undef main
uint8_t fg143_decode_row(int64_t type,const void*raw,int64_t bytes,float*out,int64_t n){uint8_t ok=gg_function_1(type,gg_view((void*)raw,bytes),gg_view(out,n),gg_view((void*)iq2xxs_grid,256),gg_view((void*)iq2xs_grid,512),gg_view((void*)iq2s_grid,1024),gg_view((void*)iq3xxs_grid,256),gg_view((void*)iq3s_grid,512),gg_view((void*)iq1s_grid,NGRID_IQ1S),gg_view((void*)ksigns_iq2xs,128),gg_view((void*)kvalues_iq4nl,16),0);if(ok&&type>=0&&type<43)atomic_fetch_add_explicit(codec_calls+type,1,memory_order_relaxed);return ok;}
__attribute__((destructor))static void fg143_codec_report(void){for(int t=0;t<43;++t){uint64_t n=atomic_load(codec_calls+t);if(n)fprintf(stderr,"FG143_CODEC type=%d calls=%llu\n",t,(unsigned long long)n);}}
