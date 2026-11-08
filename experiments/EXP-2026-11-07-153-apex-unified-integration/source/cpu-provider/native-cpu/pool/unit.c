#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdatomic.h>
int64_t fg149_meta(int64_t,int64_t);
void fg149_set(int64_t,int64_t,int64_t);
int64_t fg149_action(int64_t,int64_t,int64_t,int64_t);
static atomic_uint_fast64_t calls[19];
static void fg149_witness(int64_t mode){if(mode<0||mode>=19)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg149_unused_main
#include "../../../generated/cpu-pool.c"
#undef main
int64_t fg149_pool_control(int64_t mode,int64_t context,const uint8_t*mask,int64_t n,uint8_t*local,int64_t size){return gg_function_1(mode,context,gg_view((void*)mask,n),gg_view(local,size),0);}
__attribute__((destructor))static void report(void){for(int i=0;i<19;++i){uint64_t n=atomic_load(calls+i);if(n)fprintf(stderr,"FG149_POOL mode=%d calls=%llu\n",i,(unsigned long long)n);}}
