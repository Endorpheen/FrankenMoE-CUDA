#include "cuda-resource-schema.h"
#include "canonical-storage.h"
#include "fg14_abi.h"
#if defined(__has_include)
#  if __has_include("generated/cuda-launch.c")
#    define FG14_LAUNCH_FILE "generated/cuda-launch.c"
#  else
#    define FG14_LAUNCH_FILE "cuda-launch.generated.c"
#  endif
#endif
#ifndef FG14_LAUNCH_FILE
#  define FG14_LAUNCH_FILE "generated/cuda-launch.c"
#endif
#define main fg14_launch_generated_main
#include FG14_LAUNCH_FILE
#undef main
#ifndef FG14_LAUNCH_ENTRY
#  define FG14_LAUNCH_ENTRY gg_function_1
#endif
int64_t fg14_goose_launch(int64_t p0,int64_t p1,int64_t p2,int64_t p3,
                          int64_t p4,int64_t p5,int64_t p6) {
    return FG14_LAUNCH_ENTRY(p0,p1,p2,p3,p4,p5,p6,1);
}
int64_t fg14_launch_graph(int64_t c,int64_t g,int64_t u,int64_t upd,int64_t k) {
    return fg14_goose_launch(1,c,g,u,upd,k,0);
}
