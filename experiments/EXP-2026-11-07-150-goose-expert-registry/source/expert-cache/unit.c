#include "bridge.h"
#include <stdio.h>
#include <stdatomic.h>
uint64_t fg150_budget_double_bits(double);
int64_t fg150_budget_get(int64_t,int64_t,int64_t);
double fg150_budget_float(int64_t,int64_t,int64_t);
void fg150_budget_put(int64_t,int64_t,int64_t,int64_t);
void fg150_budget_put_float(int64_t,int64_t,double);
static atomic_uint_fast64_t counts[9];
static void fg150_cache_witness(int64_t mode){if(mode<0||mode>=9)__builtin_trap();atomic_fetch_add_explicit(counts+mode,1,memory_order_relaxed);}
#define main fg150_unused_main
#include "../../generated/expert-cache.c"
#undef main
int64_t fg150_cache(int64_t mode,int64_t context){return gg_function_1(mode,context,0);}
__attribute__((destructor))static void report(void){for(int i=0;i<9;i++){uint64_t n=atomic_load(counts+i);if(n)fprintf(stderr,"FG150_CACHE mode=%d calls=%llu\n",i,(unsigned long long)n);}}
