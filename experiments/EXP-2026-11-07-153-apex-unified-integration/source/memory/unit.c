#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include "bridge.h"
static atomic_uint_fast64_t calls[13];
static void fg153_mw(int64_t mode){if(mode<0||mode>=13)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg153_memory_unused_main
#include "../../generated/memory.c"
#undef main
int64_t fg153_memory(int64_t mode,int64_t a,int64_t b,int64_t c,int64_t d){return gg_function_1(mode,a,b,c,d,0);}
__attribute__((destructor))static void fg153_report(void){for(int i=0;i<13;i++){uint64_t n=atomic_load(calls+i);if(n)fprintf(stderr,"FG153_MEMORY mode=%d calls=%llu\n",i,(unsigned long long)n);}}
