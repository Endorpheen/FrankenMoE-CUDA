#include "cuda-resource-schema.h"
#include "canonical-storage.h"
#include "fg14_abi.h"
#if defined(__has_include)
#  if __has_include("generated/cuda-optimize.c")
#    define FG14_OPT_FILE "generated/cuda-optimize.c"
#  else
#    define FG14_OPT_FILE "cuda-optimize.generated.c"
#  endif
#endif
#ifndef FG14_OPT_FILE
#  define FG14_OPT_FILE "generated/cuda-optimize.c"
#endif
#define main fg14_optimize_generated_main
#include FG14_OPT_FILE
#undef main
#ifndef FG14_OPT_ENTRY
#  define FG14_OPT_ENTRY gg_function_1
#endif
int64_t fg14_goose_optimize(int64_t p0,int64_t p1,int64_t p2,int64_t p3,
                            int64_t p4,int64_t p5,int64_t p6) {
    return FG14_OPT_ENTRY(p0,p1,p2,p3,p4,p5,p6,1);
}
int64_t fg14_graph_optimize(int64_t n,int64_t g,int64_t p) {
    return fg14_goose_optimize(0,n,g,p,0,0,0);
}
