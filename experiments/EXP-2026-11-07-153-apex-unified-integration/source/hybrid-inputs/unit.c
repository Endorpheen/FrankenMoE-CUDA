#include "bridge.h"
#include <stdatomic.h>
#include <stdio.h>
static atomic_uint_fast64_t calls[4];
void fg153_hi_witness(int64_t mode){if(mode<0||mode>=4)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg153_hybrid_inputs_unused_main
#include "../../generated/hybrid-inputs.c"
#undef main
int64_t fg153_hybrid_inputs(int64_t mode,int64_t p){return gg_function_1(mode,p,0);}
__attribute__((destructor))static void report(void){for(int i=0;i<4;i++){uint64_t n=atomic_load_explicit(calls+i,memory_order_relaxed);if(n)fprintf(stderr,"FG153_HYBRID_INPUT mode=%d calls=%llu\n",i,(unsigned long long)n);}}
