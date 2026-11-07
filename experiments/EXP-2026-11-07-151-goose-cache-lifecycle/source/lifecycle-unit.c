#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
int64_t fg151_meta(int64_t,int64_t,int64_t,int64_t);
void fg151_put(int64_t,int64_t,int64_t,int64_t,int64_t);
int64_t fg151_action(int64_t,int64_t,int64_t,int64_t,int64_t);
static atomic_uint_fast64_t calls[7];
static void fg151_witness(int64_t mode){if(mode<0||mode>=7)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg151_lifecycle_unused_main
#include "../../generated/cache-lifecycle.c"
#undef main
int64_t fg151_lifecycle(int64_t mode,int64_t context){return gg_function_1(mode,context,0);}
__attribute__((destructor))static void fg151_report(void){for(int i=0;i<7;i++){uint64_t n=atomic_load(calls+i);if(n)fprintf(stderr,"FG151_LIFECYCLE mode=%d calls=%llu\n",i,(unsigned long long)n);}}
