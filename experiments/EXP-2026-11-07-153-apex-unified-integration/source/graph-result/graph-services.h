#pragma once
// worker-7: typed primitives for the Apex graph-result/input block.
//
// Goose owns every loop and decision (reset order, reuse predicates, input
// packing, fused-node bookkeeping). This header exposes only typed raw storage,
// OS and backend primitives over the real llama/ggml classes; it contains no
// owned algorithm and no global scratch.
//
// Result tensor slots:
//   0 t_inp_tokens, 1 t_inp_embd, 2 t_logits, 3 t_embd,
//   4 t_embd_pooled, 5 t_h_nextn
// Sampling kinds:
//   0 t_sampled, 1 t_sampled_probs, 2 t_sampled_logits, 3 t_candidates
//
// Input kinds:
//   0 llm_graph_input_embd, 1 llm_graph_input_embd_h,
//   2 llm_graph_input_pos, 3 llm_graph_input_out_ids
// Input fields per kind:
//   kind 0: 0 tokens 1 embd 2 mixed_tokens 3 mixed_slots 4 mixed_embd
//           5 scale_rows 6 n_embd (float scale_tok is read by fg153_gi_fget)
//   kind 1: 0 tokens 1 embd 2 h 3 n_embd 4 n_embd_state
//   kind 2: 0 pos 1 n_pos_per_embd 2 rope_section_order
//   kind 3: 0 out_ids 1 n_outputs

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Constants and generic tensor access.
// ---------------------------------------------------------------------------
int64_t fg153_gr_const(int64_t code); // 0 = LLAMA_MAX_LAYERS

int64_t fg153_gx_ne(int64_t t, int64_t i);
int64_t fg153_gx_nb(int64_t t, int64_t i);
int64_t fg153_gx_elem(int64_t t);
int64_t fg153_gx_nbytes(int64_t t);
int64_t fg153_gx_is_host(int64_t t);
void    fg153_gx_set_output(int64_t t);

// Direct host stores (assert the tensor buffer is host-backed).
void fg153_gx_store_i32(int64_t t, int64_t idx, int64_t v);
void fg153_gx_store_i64(int64_t t, int64_t idx, int64_t v);
void fg153_gx_store_f32(int64_t t, int64_t idx, float v);
void fg153_gx_store_f64(int64_t t, int64_t idx, double v);

// Backend stores (work for host and device buffers).
void fg153_gx_set_i32(int64_t t, int64_t idx, int64_t v);
void fg153_gx_set_i64(int64_t t, int64_t idx, int64_t v);
void fg153_gx_set_f32(int64_t t, int64_t idx, float v);
void fg153_gx_set_bytes(int64_t t, int64_t src, int64_t off, int64_t size);

void fg153_gx_assert(int64_t ok);
void fg153_gx_fail(void);
void fg153_gx_log(int64_t site, int64_t a, int64_t b);

// ---------------------------------------------------------------------------
// llm_graph_result lifecycle and collections.
// ---------------------------------------------------------------------------
int64_t fg153_gr_tensor_get(int64_t r, int64_t slot);
void    fg153_gr_tensor_set(int64_t r, int64_t slot, int64_t t);

int64_t fg153_gr_layer_inp_len(int64_t r);
void    fg153_gr_layer_inp_reset(int64_t r, int64_t n);
int64_t fg153_gr_layer_inp_get(int64_t r, int64_t i);
void    fg153_gr_layer_inp_set(int64_t r, int64_t i, int64_t t);

int64_t fg153_gr_sampling_len(int64_t r, int64_t kind);
int64_t fg153_gr_sampling_get(int64_t r, int64_t kind, int64_t i);
void    fg153_gr_sampling_clear(int64_t r, int64_t kind);

int64_t fg153_gr_inputs_len(int64_t r);
int64_t fg153_gr_input_at(int64_t r, int64_t i);
int64_t fg153_gr_inputs_add(int64_t r, int64_t raw_input); // takes ownership
void    fg153_gr_inputs_clear(int64_t r);
int64_t fg153_gi_set_input(int64_t inp, int64_t ubatch);
int64_t fg153_gi_can_reuse(int64_t inp, int64_t params);

int64_t fg153_gr_fused_len(int64_t r);
void    fg153_gr_fused_push(int64_t r, int64_t op, int64_t tensor, int64_t il);
int64_t fg153_gr_fused_op_at(int64_t r, int64_t i);
int64_t fg153_gr_fused_tensor_at(int64_t r, int64_t i);
int64_t fg153_gr_fused_il_at(int64_t r, int64_t i);
void    fg153_gr_fused_clear(int64_t r);

int64_t fg153_gr_hdmd_get(int64_t r, int64_t cur, int64_t rot);
void    fg153_gr_hdmd_set(int64_t r, int64_t cur, int64_t rot, int64_t res);
void    fg153_gr_hdmd_clear(int64_t r);

void    fg153_gr_params_clear(int64_t params_ptr);
void    fg153_gr_params_copy(int64_t dst_params, int64_t src_params);
int64_t fg153_gr_get_max_nodes(int64_t r);

int64_t fg153_gr_meta_size(int64_t r);
void    fg153_gr_meta_resize(int64_t r, int64_t bytes);
int64_t fg153_gr_ctx_init(int64_t r);
void    fg153_gr_ctx_set(int64_t r, int64_t ctx);
int64_t fg153_gr_ctx_get(int64_t r);
int64_t fg153_gr_new_graph(int64_t ctx, int64_t max_nodes);
void    fg153_gr_gf_set(int64_t r, int64_t gf);
int64_t fg153_gr_gf_get(int64_t r);
int64_t fg153_gr_tensor_overhead(void);
int64_t fg153_gr_graph_overhead(int64_t max_nodes);

// ---------------------------------------------------------------------------
// llm_graph_params typed reads.
// ---------------------------------------------------------------------------
// k: 0 arch 1 n_outputs 2 gtype 3 cvec_present 4 loras_present 5 cross_present
//    6 mctx_present 7 samplers_size 8 embeddings 9 embeddings_nextn
//    10 embeddings_nextn_masked 11 causal_attn 12 nextn_layer_offset
//    13 pooling_type 14 kv_unified 15 flash_attn 16 training 17 n_ctx
//    18 n_outputs_max_per_seq
int64_t fg153_gp_get(int64_t params, int64_t k, int64_t i);
int64_t fg153_gp_sampler_key(int64_t params, int64_t i);
int64_t fg153_gp_sampler_val(int64_t params, int64_t i);
int64_t fg153_gp_layer_inp_size(int64_t params);
int64_t fg153_gp_layer_inp_at(int64_t params, int64_t i);
int64_t fg153_gp_ubatch(int64_t params); // address of params->ubatch

// ---------------------------------------------------------------------------
// llama_ubatch typed reads.
// ---------------------------------------------------------------------------
// k: 0 n_tokens 1 n_seq_tokens 2 n_seqs 3 n_seqs_unq 4 n_pos 5 equal_seqs
//    6 is_mixed 7 has_token 8 has_embd 9 has_embd_state 10 has_pos
//    11 has_output 12 has_data 13 token(i) 14 type(i) 15 pos(i) 16 output(i)
//    17 n_seq_id(i) 18 seq_id_unq(i) 19 token_ptr 20 embd_ptr
//    21 embd_state_ptr 22 pos_ptr 23 output_ptr 24 type_ptr
int64_t fg153_gu_get(int64_t u, int64_t k, int64_t i);
int64_t fg153_gu_seq_id(int64_t u, int64_t i, int64_t s);

// ---------------------------------------------------------------------------
// Graph input object typed reads/writes.
// ---------------------------------------------------------------------------
int64_t fg153_gi_get(int64_t inp, int64_t kind, int64_t k, int64_t i);
float   fg153_gi_fget(int64_t inp, int64_t kind, int64_t k); // kind 0, k 7 = scale_tok
void    fg153_gi_set(int64_t inp, int64_t kind, int64_t k, int64_t v);

// ---------------------------------------------------------------------------
// llm_graph_context resource metadata reads.
// ---------------------------------------------------------------------------
// k: 0 arch
//    hparams: 1 n_embd 2 n_layer() 3 n_layer_nextn 4 n_rot() 5 n_head()
//    6 n_head_kv() 7 n_embd_head_k() 8 n_embd_k_gqa() 9 n_embd_head_v()
//    10 n_embd_v_gqa() 11 n_expert 12 n_expert_used() 15 rope_type
//    cparams: 16 n_ctx 23 n_ctx_orig_yarn 24 pooling_type 25 warmup
//    ubatch/params: 26 n_tokens 27 n_outputs 28 sched 29 backend_cpu 30 cvec
//    31 loras 32 mctx 33 cross 34 moe_cache 35 prec_policy 36 hdmd 37 res
int64_t fg153_gh_get(int64_t params, int64_t k, int64_t i);

// Float metadata reads (IEEE values preserved via f32 arguments).
// k: 0 rope_freq_base 1 rope_freq_scale 2 yarn_ext_factor 3 yarn_attn_factor
//    4 yarn_beta_fast 5 yarn_beta_slow 6 f_norm_eps 7 f_norm_rms_eps
float fg153_gh_fget(int64_t params, int64_t k);

// Metadata sink used by graph-context.goose. The layout is declared in
// graph-meta.h; Goose writes scalar values by index and the native adapter
// binds pointer/map members that cannot be derived numerically.
void fg153_gc_clear(int64_t out, int64_t n);
void fg153_gc_put(int64_t out, int64_t index, int64_t value);
void fg153_gc_putf(int64_t out, int64_t index, float value);

#ifdef __cplusplus
}
#endif
