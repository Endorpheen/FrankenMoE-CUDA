#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
int64_t fg145_meta(int64_t,int64_t);
float fg145_float(int64_t,int64_t);
int64_t fg145_action(int64_t,int64_t,int64_t,int64_t);
static atomic_uint_fast64_t api_calls[34];
static void fg145_witness(int64_t mode){if(mode<0||mode>=34)__builtin_trap();atomic_fetch_add_explicit(api_calls+mode,1,memory_order_relaxed);}
_Static_assert(sizeof(uintptr_t)==sizeof(int64_t),"opaque ABI needs64bits");
#define main fg145_unused_main
#include "../../../generated/cpu-backend-api.c"
#undef main
int64_t fg145_backend_api(int64_t mode,int64_t context){return gg_function_1(mode,context,0);}
__attribute__((destructor))static void fg145_report(void){for(int i=0;i<34;++i){uint64_t n=atomic_load(api_calls+i);if(n)fprintf(stderr,"FG145_BACKEND mode=%d calls=%llu\n",i,(unsigned long long)n);}}
