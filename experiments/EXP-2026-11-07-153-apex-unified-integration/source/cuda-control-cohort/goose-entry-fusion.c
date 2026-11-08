#include "cuda-resource-schema.h"
#include "canonical-storage.h"
#include "fg14_abi.h"
#if defined(__has_include)
#  if __has_include("generated/cuda-fusion.c")
#    define FG14_FUSION_FILE "generated/cuda-fusion.c"
#  else
#    define FG14_FUSION_FILE "cuda-fusion.generated.c"
#  endif
#endif
#ifndef FG14_FUSION_FILE
#  define FG14_FUSION_FILE "generated/cuda-fusion.c"
#endif
#define main fg14_fusion_generated_main
#include FG14_FUSION_FILE
#undef main
#ifndef FG14_FUSION_ENTRY
#  define FG14_FUSION_ENTRY gg_function_1
#endif
int64_t fg14_goose_fusion(int64_t p0,int64_t p1,int64_t p2,int64_t p3,
                          int64_t p4,int64_t p5,int64_t p6) {
    return FG14_FUSION_ENTRY(p0,p1,p2,p3,p4,p5,p6,1);
}
int64_t fg14_can_fuse(int64_t g,int64_t i,int64_t ops,int64_t n,
                      int64_t un,int64_t nun) {
    return fg14_goose_fusion(0,g,i,ops,n,un,nun);
}
int64_t fg14_try_fuse(int64_t c,int64_t g,int64_t i) {
    return fg14_goose_fusion(1,c,g,i,0,0,0);
}
int64_t fg14_match_shared(int64_t g,int64_t r,int64_t s) {
    return fg14_goose_fusion(2,g,r,s,0,0,0);
}
