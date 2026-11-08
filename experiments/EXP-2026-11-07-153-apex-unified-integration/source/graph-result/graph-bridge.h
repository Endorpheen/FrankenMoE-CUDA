#pragma once
// worker-7: bridge between the real llama graph classes and the Goose modules.
//
// Production replay (see replay.py) preserves the original native bodies below
// each injected hook. Building with -DFG153_NATIVE_ROLLBACK compiles the hooks
// out and restores the untouched native implementation.
#include "graph-services.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Goose exports (wrappers around the generated C entry functions).
// res: 0 reset(r,max_nodes), 1 set_inputs(r,ubatch), 2 set_outputs(r,params),
//      3 can_reuse(r,new_params,stored_params,debug),
//      4 add_input(r,input_owner_ptr), 5 add_fused_node(r,op,tensor,il),
//      6 get_hdmd_input(r,cur,rot), 7 set_hdmd_input(r,cur,rot,res),
//      8 set_params(r,params)
int64_t fg153_goose_res(int64_t op, int64_t r, int64_t a, int64_t b, int64_t c, int64_t d);

// inp: 0 embd_set 1 embd_reuse 2 embd_h_set 3 embd_h_reuse
//      4 pos_set 5 pos_reuse 6 out_ids_set 7 out_ids_reuse
// For *_set the second handle is a llama_ubatch; for *_reuse it is a
// llm_graph_params.
int64_t fg153_goose_inp(int64_t op, int64_t inp, int64_t x, int64_t y, int64_t z, int64_t w);

// ctx: 0 meta_build(params, meta_out), 1 crop_before_nextn(params,inp_out_ids),
//      2 crop_after_nextn(params,inp_out_ids), 3 meta_valid(params), 4 debug_enabled()
int64_t fg153_goose_ctx(int64_t op, int64_t params, int64_t out, int64_t a, int64_t b, int64_t c);

// Metadata layout written by graph-context.goose (ints followed by floats).
struct fg153_graph_meta {
    int64_t arch;
    int64_t n_embd;
    int64_t n_layer;
    int64_t n_layer_nextn;
    int64_t n_rot;
    int64_t n_ctx;
    int64_t n_head;
    int64_t n_head_kv;
    int64_t n_embd_head_k;
    int64_t n_embd_k_gqa;
    int64_t n_embd_head_v;
    int64_t n_embd_v_gqa;
    int64_t n_expert;
    int64_t n_expert_used;
    int64_t n_tokens;
    int64_t n_outputs;
    int64_t n_ctx_orig;
    int64_t pooling_type;
    int64_t rope_type;
    int64_t sched;
    int64_t backend_cpu;
    int64_t cvec;
    int64_t loras;
    int64_t mctx;
    int64_t cross;
    int64_t moe_cache;
    int64_t prec_policy;
    int64_t hdmd;
    int64_t res;
    float   freq_base;
    float   freq_scale;
    float   ext_factor;
    float   attn_factor;
    float   beta_fast;
    float   beta_slow;
    float   norm_eps;
    float   norm_rms_eps;
};

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
int fg153_graph_result_enabled(void);
#ifdef __cplusplus
}
#endif
