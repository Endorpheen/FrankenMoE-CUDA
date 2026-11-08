#include "bridge.h"
#include <stdatomic.h>
#include <stdio.h>
static atomic_uint_fast64_t calls[10];
void fg153_bl_witness(int64_t mode){if(mode<0||mode>=10)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg153_buffer_lifecycle_unused_main
#include "../../generated/buffer-lifecycle.c"
#undef main
int64_t fg153_buffer_lifecycle(int64_t mode,int64_t a,int64_t b){return gg_function_1(mode,a,b,0);}
__attribute__((destructor))static void report(void){for(int i=0;i<10;i++){uint64_t n=atomic_load_explicit(calls+i,memory_order_relaxed);if(n)fprintf(stderr,"FG153_BUFFER_LIFECYCLE mode=%d calls=%llu\n",i,(unsigned long long)n);}}
