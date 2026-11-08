// worker-4: EXP107 context execution service ABI (UNVERIFIED PROPOSAL).
// C linkage used by context-execution.goose. All handles are opaque i64:
//   ctx   = llama_context *
//   b     = const llama_batch_ext *
//   u     = const llama_ubatch *
//   m     = llama_memory_context_i *
//   r     = llm_graph_result *
//   g     = ggml_cgraph *
//   t     = const ggml_tensor *
//   be    = ggml_backend_t
//   gp    = const llm_graph_params * (owned by the service scratch slot)
//
// Integration requirements (parent, Codex):
//   1. add `friend struct fg107_context_access;` to class llama_context
//      (single line, no behaviour change; rollback = remove the line);
//   2. compile context-services.cpp as part of the llama target;
//   3. route llama_context::decode to the Goose export fg107_goose_decode;
//   4. register fg107_sched_copy_experts_cb after sched_new (keeps the
//      expert-overlap callback that EXP107 relies on).
//
// llm_graph_result accessors (op 0..9) are derived from call sites in
// active-llama-context.cpp, not from a header in inputs/ (llama-graph.h is not
// part of the snapshot). Parent must confirm names/signatures against the
// real header. Everything else uses headers present in inputs/.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- ctx(ctx, field, a) ---------------------------------------------------
// 0 memory_present 1 sched_present 2 n_ctx 3 n_batch 4 n_ubatch 5 n_seq_max
// 6 n_vocab 7 n_embd 8 n_embd_out 9 n_cls_out 10 embeddings
// 11 embeddings_nextn 12 embeddings_nextn_masked 13 causal_attn 14 kv_unified
// 15 pipeline_parallel 16 graph_reuse_disabled 17 graph_type 18 pooling_type
// 19 arch_is_t5 20 graph_has_logits 21 n_outputs_max 22 n_outputs_max_per_seq
// 23 has_samplers 24 layer_inp_count 25 n_outputs 26 no_perf 27 n_threads
// 28 n_threads_batch 29 threadpool(a=batched) 30 lazy_pp 31 max_seq
// 32 pos_max 33 time_us 34 max_nodes(a=n_tokens) 35 layer_inp_enabled(a=il)
// 36 sampler_has(a=seq) 37 count_inc(a=seq) 39 last_ret
int64_t fg107_ctx(int64_t ctx, int64_t field, int64_t a);

// ctx_set(ctx, field, v): 0 n_outputs 1 note_reused 2 last_error
// 3 samplers_begin 4 last_ret 5 sampler_counts_reset
int64_t fg107_ctx_set(int64_t ctx, int64_t field, int64_t v);

// perf(ctx, op, field, value): op 0 get, 1 set.
// field 0 queued 1 t_start 2 t_compute_start 3 n_eval 4 t_eval 5 n_p_eval
// 6 t_p_eval 7 has_evaluated 8 t_load 9 no_perf(read only)
int64_t fg107_perf(int64_t ctx, int64_t op, int64_t field, int64_t value);

// log(ctx, site, a, b): fixed EXP107 messages, site 1..20 (see .cpp).
int64_t fg107_log(int64_t ctx, int64_t site, int64_t a, int64_t b);

// batch(b, field, i, s): 0 n_tokens 1 n_embd 2 n_embd_inp 3 token_output(i)
// 4 token_n_seq(i) 5 token_seq(i,s)
int64_t fg107_batch(int64_t b, int64_t field, int64_t i, int64_t s);

// ub(u, field, i, s): 0 n_tokens 1 output(i) 2 n_seq_id(i) 3 seq_id(i,s)
// 4 pos(i) 5 n_seqs_unq 6 seq_id_unq(s) 7 seq_idx(s) 8 batch_idxs_count
// 9 batch_idx(i)
int64_t fg107_ub(int64_t u, int64_t field, int64_t i, int64_t s);

// balloc(ctx, op, a, b): 0 init(a=batch,b=output_all) 1 n_tokens 2 n_outputs
// 3 out_ids_size 4 out_id(a) 5 out_swap(a,b)
int64_t fg107_balloc(int64_t ctx, int64_t op, int64_t a, int64_t b);

// mem(ctx, op, a, b, d): 0 init_batch(a=n_ubatch,b=embd_all) 1 init_full
// 2 init_update(a=optimize) 3 seq_rm(a=seq,b=p0,d=p1)
int64_t fg107_mem(int64_t ctx, int64_t op, int64_t a, int64_t b, int64_t d);

// mctx(m, op, a): 0 status 1 apply 2 next 3 ubatch
int64_t fg107_mctx(int64_t m, int64_t op, int64_t a);

int64_t fg107_res_prev(int64_t ctx);
int64_t fg107_res_prev_active(int64_t ctx);
int64_t fg107_res_set_prev_active(int64_t ctx, int64_t r);
int64_t fg107_res_reset_all_prev(int64_t ctx);

// All decode-path services take a per-call frame handle (FG107ExecFrame*),
// not a llama_context*. The frame owns the memory context, graph params,
// sampler counters and error slots; it is stack-owned by the parent glue and
// destroyed deterministically. No process-wide scratch exists.
//
// res(frame, r, op, a): 0 gf 1 can_reuse 2 reset 3 set_inputs(a=ubatch)
// 4 logits 5 embd 6 embd_pooled 7 h_nextn 8 layer_inp(a=il) 9 sampling_count(a=kind)
int64_t fg107_res(int64_t frame, int64_t r, int64_t op, int64_t a);
// res_sampling_tensor(r, kind, i): i < 0 -> element count, else tensor i
int64_t fg107_res_sampling_tensor(int64_t r, int64_t kind, int64_t i);

int64_t fg107_graph_params(int64_t ctx, int64_t r, int64_t u, int64_t m, int64_t gtype);
int64_t fg107_model_build_graph(int64_t ctx, int64_t gp);
int64_t fg107_graph_reserve(int64_t ctx, int64_t n_tokens, int64_t n_seqs,
                            int64_t n_outputs, int64_t m, int64_t lazy);

// graph(g, op, a): 0 n_nodes 1 node(a)
int64_t fg107_graph(int64_t g, int64_t op, int64_t a);

// sched(ctx, op, a, b, d): 0 reset 1 set_eval_cb 2 alloc_graph(a)
// 3 synchronize 4 tensor_backend(a) 5 backend_count 6 compute_async(a)
// 7 reserve 8 copy_experts_reset 9 apply_threadpool(a) 10 apply_n_threads(a)
// 11 threads(a=batched) 12 threadpool(a=batched) 13 buffer_size(a=backend)
// 14 record_buf_size(a=backend) 15 backend_at(a=i)
int64_t fg107_sched(int64_t ctx, int64_t op, int64_t a, int64_t b, int64_t d);

// tensor(t, field, i): 0 op 1 src(i) 2 ne(i) 3 nb(i) 4 nelements 5 nbytes
// 6 is_contiguous 7 data
int64_t fg107_tensor(int64_t t, int64_t field, int64_t i);

int64_t fg107_tensor_get_async(int64_t be, int64_t t, int64_t dst, int64_t off, int64_t size);
int64_t fg107_tensor_set_async(int64_t be, int64_t t, int64_t src, int64_t off, int64_t size);
int64_t fg107_backend_synchronize(int64_t be);

// moe(ctx, op, a, b, d, e, f): 0 cache_present 1 cache_copy(a=be,b=src,d=dst,e=graph)
// 2 cache_copy_experts(a=be,b=src,d=dst,e=first,f=last)
int64_t fg107_moe(int64_t ctx, int64_t op, int64_t a, int64_t b, int64_t d, int64_t e, int64_t f);

int64_t fg107_op_mul_mat_id(void);

// out(ctx, op, a, b, d): 0 buf_size 1 buf_alloc(a=size) 2 buf_base
// 3 logits_ptr 4 logits_size 5 embd_ptr 6 embd_size 7 nextn_ptr 8 nextn_size
// 9 layer_ptr(a=il) 10 layer_size(a=il) 11 layer_has_data(a=il)
// 12 embd_seq_size 13 embd_seq_clear 14 embd_seq_resize(a=seq,b=n)
// 15 embd_seq_ptr(a=seq) 16 sampling_prepare(a=n_outputs_max) 17 sampling_clear
// 18 sampling_ptr(a=kind) 19 sampling_size(a=kind) 20 sampling_count_capacity(a=kind)
// 21 sampling_count_set(a=kind,b=row,d=n) 22 sampling_elem_size(a=kind)
// 23 sampling_swap_scalar(a=kind,b=i,d=j)
int64_t fg107_out(int64_t ctx, int64_t op, int64_t a, int64_t b, int64_t d);
// which: 0 logits 1 embd 2 embd_nextn 3 layer(il) 4 samp.logits 5 samp.probs
// 6 samp.sampled 7 samp.candidates; offset in bytes, count in elements
int64_t fg107_out_bind(int64_t ctx, int64_t which, int64_t il, int64_t off, int64_t count);
// swap element i and j inside the selected view
int64_t fg107_out_view_swap(int64_t ctx, int64_t which, int64_t il, int64_t i, int64_t j);

// exec(ctx, op, a, b, d): 0 output_ids_init(a=n) 1 output_ids_size
// 2 output_ids_get(a=i) 3 output_ids_set(a=i,b=v) 4 output_ids_fill(a=v)
// 5 swap_count 6 swap_get(a=k,b=which) 7 swap_push(a=i0,b=i1) 8 swaps_clear
// 9 embd_idx_count 10 embd_idx_get(a=i) 11 embd_idx_append_batch(a=ubatch)
// 12 embd_idx_clear 13 embd_idx_swap(a=i,b=j) 14 copy_ids 15 copy_ids_set(a=t)
// 16 copy_ids_data_resize(a=n) 17 copy_ids_data_ptr 19 copy_used_size
// 20 copy_used_assign(a=n) 21 copy_used_get(a=i) 22 copy_used_set(a=i,b=v)
int64_t fg107_exec(int64_t ctx, int64_t op, int64_t a, int64_t b, int64_t d);
int64_t fg107_exec_copy_id_at(int64_t ctx, int64_t i);

// Expert-overlap callback glue. Keeps sched_copy_experts registered after
// sched_new (active-llama-context.cpp:754).
int64_t fg107_sched_set_copy_callback(int64_t ctx);
// Abort-callback registration glue (:1386-1401), kept as a backend primitive.
int64_t fg107_ctx_set_abort_callback(int64_t ctx, int64_t cb, int64_t data);

// ---- resource lifecycle / checkpoint ABI (context-resource.goose) ----------
// rq(frame, field, a) / rq_set(frame, field, v): field tables documented in
// context-resource.goose. r(frame, op, a, b, c): op tables documented there.
int64_t fg107_rq(int64_t frame, int64_t field, int64_t a);
int64_t fg107_rq_set(int64_t frame, int64_t field, int64_t v);
int64_t fg107_r(int64_t frame, int64_t op, int64_t a, int64_t b, int64_t c);

// Strong exports required from context-resource.goose:
//   fg107_goose_resource_init/release, fg107_goose_sched_new/sched_reserve,
//   fg107_goose_state_{get,set}_data, fg107_goose_state_get_size,
//   fg107_goose_state_seq_{get,set}_data, fg107_goose_state_seq_get_size

// Strong export from the Goose module. A missing symbol is a link error; the
// only alternative is the explicit FG107_NATIVE_ROLLBACK build.
int64_t fg107_goose_sched_copy_experts(int64_t ctx, int64_t be, int64_t src, int64_t dst, int64_t g);

#ifdef __cplusplus
}
#endif
