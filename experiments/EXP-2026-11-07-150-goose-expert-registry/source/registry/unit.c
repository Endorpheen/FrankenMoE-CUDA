#include "bridge.h"
#include <stdio.h>
#include <stdatomic.h>
static atomic_uint_fast64_t counts[12];
static void fg150_registry_witness(int64_t mode){if(mode<0||mode>=12)__builtin_trap();atomic_fetch_add_explicit(counts+mode,1,memory_order_relaxed);}
#define main fg150_unused_main
#include "../../../generated/cpu-registry.c"
#undef main
int64_t fg150_registry(int64_t mode,int64_t context){return gg_function_1(mode,context,0);}
__attribute__((destructor))static void fg150_report(void){for(int i=0;i<12;i++){uint64_t n=atomic_load(counts+i);if(n)fprintf(stderr,"FG150_REGISTRY mode=%d calls=%llu\n",i,(unsigned long long)n);}}
