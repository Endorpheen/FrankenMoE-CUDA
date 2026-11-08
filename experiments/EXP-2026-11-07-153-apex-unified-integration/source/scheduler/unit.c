#include "bridge.h"
#define main fg153_scheduler_unused_main
#include "../../generated/scheduler/kernel.c"
#undef main
int64_t fg153_scheduler(int64_t mode,int64_t s,int64_t a,int64_t b){return gg_function_1(mode,s,a,b,0);}
