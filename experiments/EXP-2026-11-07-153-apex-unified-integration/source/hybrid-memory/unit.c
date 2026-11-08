#include "bridge.h"
#include <stdatomic.h>
#include <stdio.h>
static atomic_uint_fast64_t calls[21];
void fg153_hm_witness(int64_t mode){if(mode<0||mode>=21)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg153_hybrid_unused_main
#include "../../generated/hybrid-memory.c"
#undef main
int64_t fg153_hybrid(int64_t mode,int64_t p,int64_t a,int64_t b,int64_t c,int64_t d){return gg_function_1(mode,p,a,b,c,d,0);}
__attribute__((destructor))static void report(void){for(int i=0;i<21;i++){uint64_t n=atomic_load_explicit(calls+i,memory_order_relaxed);if(n)fprintf(stderr,"FG153_HYBRID mode=%d calls=%llu\n",i,(unsigned long long)n);}}
