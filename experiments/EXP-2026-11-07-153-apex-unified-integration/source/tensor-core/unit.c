#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include "bridge.h"
static atomic_uint_fast64_t counts[14];
static void fg153_witness(int64_t mode){if(mode<0||mode>=14)__builtin_trap();atomic_fetch_add_explicit(&counts[mode],1,memory_order_relaxed);}
#define main fg153_unused_main
#include "../../generated/tensor-core.c"
#undef main
int64_t fg153_entry(int64_t mode,int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f){return gg_function_1(mode,a,b,c,d,e,f,0);}
__attribute__((destructor))static void fg153_report(void){for(int i=0;i<14;i++){uint64_t n=atomic_load(&counts[i]);if(n)fprintf(stderr,"FG153_TENSOR_CORE mode=%d calls=%llu\n",i,(unsigned long long)n);}}
