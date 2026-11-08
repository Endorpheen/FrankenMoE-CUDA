#include "bridge.h"
#include <stdatomic.h>
static _Atomic uint64_t build_calls,bind_calls,policy_calls;
#define main fg153_apex_unused_main
#include "../../generated/apex-model/kernel.c"
#undef main
int64_t fg153_apex_build(int64_t owner){atomic_fetch_add_explicit(&build_calls,1,memory_order_relaxed);return gg_function_1(owner,0);}
_Static_assert(sizeof(gg_type_6)==16,"Pair layout");
_Static_assert(_Alignof(gg_type_6)==1,"Pair packing");
_Static_assert(offsetof(gg_type_6,f1)==8,"Pair second field");
_Static_assert(sizeof(gg_type_7)==24,"Triple layout");
_Static_assert(sizeof(gg_type_9)==2048,"Expert array layout");
void fg153_apex_bind(int64_t env,int64_t mode){atomic_fetch_add_explicit(mode?&policy_calls:&bind_calls,1,memory_order_relaxed);gg_function_122(env,mode,0); }
int64_t fg153_apex_probe(int64_t owner,int64_t mode){return gg_function_120(owner,mode,0); }
uint64_t fg153_apex_counts(int64_t mode){return atomic_load_explicit(mode==0?&build_calls:mode==1?&bind_calls:&policy_calls,memory_order_relaxed); }
__attribute__((destructor))static void fg153_apex_report(void){uint64_t b=fg153_apex_counts(0),t=fg153_apex_counts(1),p=fg153_apex_counts(2);if(b||t||p)fprintf(stderr,"FG153_APEX_MODEL build=%llu bind=%llu recurrent_policy=%llu\n",(unsigned long long)b,(unsigned long long)t,(unsigned long long)p); }
