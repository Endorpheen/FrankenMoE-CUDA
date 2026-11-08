/* worker-14 / APX025: strong entry wrappers for generated Goose units. */
#include "cuda-resource-schema.h"
#include "canonical-storage.h"
#include "fg14_abi.h"

#if defined(__has_include)
#  if __has_include("generated/cuda-resource.c")
#    define FG14_RES_FILE "generated/cuda-resource.c"
#  else
#    define FG14_RES_FILE "cuda-resource.generated.c"
#  endif
#endif
#ifndef FG14_RES_FILE
#  define FG14_RES_FILE "generated/cuda-resource.c"
#endif
#define main fg14_resource_generated_main
#include FG14_RES_FILE
#undef main
#ifndef FG14_RES_ENTRY
#  define FG14_RES_ENTRY gg_function_1
#endif

int64_t fg14_goose_resource(int64_t p0, int64_t p1, int64_t p2, int64_t p3,
                            int64_t p4, int64_t p5, int64_t p6) {
    return FG14_RES_ENTRY(p0, p1, p2, p3, p4, p5, p6, 1);
}
int64_t fg14_res_ctx_new(int64_t d) { return fg14_goose_resource(0,d,0,0,0,0,0); }
int64_t fg14_res_ctx_attach(int64_t n,int64_t d) { return fg14_goose_resource(1,n,d,0,0,0,0); }
int64_t fg14_res_ctx_free(int64_t c) { return fg14_goose_resource(2,c,0,0,0,0,0); }
int64_t fg14_res_ctx_get(int64_t c,int64_t k) { return fg14_goose_resource(3,c,k,0,0,0,0); }
int64_t fg14_res_ctx_set(int64_t c,int64_t k,int64_t v) { return fg14_goose_resource(4,c,k,v,0,0,0); }
int64_t fg14_res_stream(int64_t c,int64_t d,int64_t s) { return fg14_goose_resource(5,c,d,s,0,0,0); }
int64_t fg14_res_cublas(int64_t c,int64_t d,int64_t s) { return fg14_goose_resource(6,c,d,s,0,0,0); }
int64_t fg14_res_pool(int64_t c,int64_t d,int64_t s) { return fg14_goose_resource(7,c,d,s,0,0,0); }
int64_t fg14_res_pool_alloc(int64_t c,int64_t d,int64_t s,int64_t z,int64_t o) { return fg14_goose_resource(8,c,d,s,z,o,0); }
int64_t fg14_res_pool_free(int64_t c,int64_t d,int64_t s,int64_t p,int64_t z) { return fg14_goose_resource(9,c,d,s,p,z,0); }
int64_t fg14_res_graph(int64_t c,int64_t k) { return fg14_goose_resource(10,c,k,0,0,0,0); }
int64_t fg14_res_graph_get(int64_t g,int64_t k) { return fg14_goose_resource(11,g,k,0,0,0,0); }
int64_t fg14_res_graph_set(int64_t g,int64_t k,int64_t v) { return fg14_goose_resource(12,g,k,v,0,0,0); }
int64_t fg14_res_graph_node_props(int64_t g,int64_t cg) { return fg14_goose_resource(13,g,cg,0,0,0,0); }
int64_t fg14_res_graph_invalidate(int64_t c) { return fg14_goose_resource(14,c,0,0,0,0,0); }
int64_t fg14_res_conc_new(int64_t c,int64_t n) { return fg14_goose_resource(15,c,n,0,0,0,0); }
int64_t fg14_res_conc_free(int64_t c) { return fg14_goose_resource(16,c,0,0,0,0,0); }
int64_t fg14_res_conc_get(int64_t c,int64_t k) { return fg14_goose_resource(17,c,k,0,0,0,0); }
int64_t fg14_res_conc_set(int64_t c,int64_t k,int64_t v) { return fg14_goose_resource(18,c,k,v,0,0,0); }
int64_t fg14_res_conc_stream_get(int64_t c,int64_t t) { return fg14_goose_resource(19,c,t,0,0,0,0); }
int64_t fg14_res_conc_stream_set(int64_t c,int64_t t,int64_t s) { return fg14_goose_resource(20,c,t,s,0,0,0); }
int64_t fg14_res_conc_order_len(int64_t c) { return fg14_goose_resource(21,c,0,0,0,0,0); }
int64_t fg14_res_conc_order_at(int64_t c,int64_t i) { return fg14_goose_resource(22,c,i,0,0,0,0); }
int64_t fg14_res_conc_order_push(int64_t c,int64_t t) { return fg14_goose_resource(23,c,t,0,0,0,0); }
int64_t fg14_res_conc_join_event(int64_t c,int64_t i) { return fg14_goose_resource(24,c,i,0,0,0,0); }
int64_t fg14_res_conc_valid(int64_t c) { return fg14_goose_resource(25,c,0,0,0,0,0); }
int64_t fg14_res_stream_ctx_reset(int64_t c) { return fg14_goose_resource(26,c,0,0,0,0,0); }
int64_t fg14_res_stream_ctx_size(int64_t c) { return fg14_goose_resource(27,c,0,0,0,0,0); }
int64_t fg14_res_stream_ctx_key_at(int64_t c,int64_t i) { return fg14_goose_resource(28,c,i,0,0,0,0); }
int64_t fg14_res_stream_ctx_lookup(int64_t c,int64_t t) { return fg14_goose_resource(29,c,t,0,0,0,0); }
int64_t fg14_res_stream_ctx_insert(int64_t c,int64_t t,int64_t e) { return fg14_goose_resource(30,c,t,e,0,0,0); }
int64_t fg14_res_graph_any_enabled(int64_t c) { return fg14_goose_resource(31,c,0,0,0,0,0); }
int64_t fg14_res_graph_any_instance(int64_t c) { return fg14_goose_resource(32,c,0,0,0,0,0); }
