#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include "bridge.h"
static atomic_uint_fast64_t counts[8];
static void fg153_bw(int64_t mode){if(mode<0||mode>=8)__builtin_trap();atomic_fetch_add_explicit(counts+mode,1,memory_order_relaxed);}
#define main fg153_buffers_unused_main
#include "../../generated/buffers.c"
#undef main
int64_t fg153_buffers(int64_t mode,int64_t a,int64_t b,int64_t c){return gg_function_1(mode,a,b,c,0);}
__attribute__((destructor))static void fg153_report(void){for(int i=0;i<8;i++){uint64_t n=atomic_load(counts+i);if(n)fprintf(stderr,"FG153_BUFFERS mode=%d calls=%llu\n",i,(unsigned long long)n);}}
