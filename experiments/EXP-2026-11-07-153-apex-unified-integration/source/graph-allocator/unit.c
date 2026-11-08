#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include "bridge.h"
#include "../memory/bridge.h"
static atomic_uint_fast64_t counts[11];
static void fg153_gw(int64_t mode){if(mode<0||mode>=11)__builtin_trap();atomic_fetch_add_explicit(counts+mode,1,memory_order_relaxed);}
#define main fg153_graphalloc_unused_main
#include "../../generated/graph-allocator.c"
#undef main
int64_t fg153_graphalloc(int64_t mode,int64_t a,int64_t b,int64_t c,int64_t d,int64_t e){return gg_function_1(mode,a,b,c,d,e,0);}
__attribute__((destructor))static void fg153_report(void){for(int i=0;i<11;i++){uint64_t n=atomic_load(counts+i);if(n)fprintf(stderr,"FG153_GRAPH_ALLOC mode=%d calls=%llu\n",i,(unsigned long long)n);}}
