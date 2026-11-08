// worker-4: EXP107 context execution services (UNVERIFIED PROPOSAL).
// Implements worker-4/context-services.h for context-execution.goose.
// Not a decode wrapper: it exposes primitives and state accessors only.
//
// Integration (parent, Codex):
//   * add `friend struct fg107_context_access;` to class llama_context;
//   * compile this TU inside the llama target;
//   * call fg107_sched_set_copy_callback(ctx) right after sched_new so the
//     expert-overlap copy callback is preserved (active-llama-context.cpp:754);
//   * route llama_context::decode to the Goose export fg107_goose_decode.
// Rollback: drop this TU + the friend line; nothing else changes.
//
// llm_graph_result / llm_graph_params / llama_moe_cache / llama_sampler_backend_begin
// declarations are not part of inputs/; they are used exactly as in
// active-llama-context.cpp and must be confirmed against the real headers.
#include "llama-context.h"
#include "llama-batch.h"
#include "llama-memory.h"
#include "llama-model.h"
#include "llama-moe-cache.h"
#include "llama-sampler.h"
#include "ggml-backend.h"
#include "context-execution-frame.hpp"
#include "fg107-route.h"
#include "fgcio-context-adapter.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

// Strong Goose export: the callback is mandatory unless the explicit native
// rollback build (FG107_NATIVE_ROLLBACK) is selected. A missing export is a
// link error, never a silent return false.
extern "C" int64_t fg107_goose_sched_copy_experts(int64_t, int64_t, int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_resolve_fused(int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_realloc_policy(int64_t, int64_t, int64_t, int64_t, int64_t);

using i64 = int64_t;

// Per-decode frame accessor. No process-wide scratch: every decode call owns
// its memory context, graph params, sampler counters and error slots, and the
// frame destructor (unique_ptr members) frees them deterministically.
static FG107ExecFrame & fr(i64 h) {
    GGML_ASSERT(h != 0);
    return *(FG107ExecFrame *) (intptr_t) h;
}

struct fg107_context_access {
    static llama_cparams       & cp   (llama_context * c) { return c->cparams; }
    static const llama_model   & model(llama_context * c) { return c->model; }
    static ggml_backend_sched_t sched(llama_context * c) { return c->sched.get(); }
    static bool sched_present(llama_context * c) { return c->sched != nullptr; }
    static llama_memory_i * mem(llama_context * c) { return c->memory.get(); }
    static llama_batch_allocr * balloc(llama_context * c) { return c->balloc.get(); }
    static llama_moe_cache * moe(llama_context * c) { return c->moe_cache.get(); }
    static uint32_t & n_outputs(llama_context * c) { return c->n_outputs; }
    static void note_reused(llama_context * c) { c->n_reused++; }
    static bool & graph_has_logits(llama_context * c) { return c->graph_has_logits; }
    static bool & graph_reuse_disable(llama_context * c) { return c->graph_reuse_disable; }
    static buffer_view<float> & logits(llama_context * c) { return c->logits; }
    static buffer_view<float> & embd(llama_context * c) { return c->embd; }
    static buffer_view<float> & embd_nextn(llama_context * c) { return c->embd_nextn; }
    static std::vector<buffer_view<float>> & layer_inp(llama_context * c) { return c->embd_layer_inp; }
    static std::vector<int32_t> & output_ids(llama_context * c) { return c->output_ids; }
    static std::vector<int32_t> & embd_batch_idxs(llama_context * c) { return c->embd_batch_idxs; }
    static std::vector<llama_context::swap_info> & swaps(llama_context * c) { return c->output_swaps; }
    static decltype(llama_context::sampling) & sampling(llama_context * c) { return c->sampling; }
    static std::map<llama_seq_id, std::vector<float>> & embd_seq(llama_context * c) { return c->embd_seq; }
    static ggml_backend_buffer_ptr & buf_output(llama_context * c) { return c->buf_output; }
    static decltype(llama_context::gf_res_prev) & gf_prev(llama_context * c) { return c->gf_res_prev; }
    static llm_graph_result_ptr & gf_reserve(llama_context * c) { return c->gf_res_reserve; }
    static llm_graph_result *& prev_active(llama_context * c) { return c->gf_res_prev_active; }
    static decltype(llama_context::copy_experts) & copy_experts(llama_context * c) { return c->copy_experts; }
    static std::vector<ggml_backend_t> & backends(llama_context * c) { return c->backend_ptrs; }
    static std::vector<size_t> & buf_exp_size(llama_context * c) { return c->backend_buf_exp_size; }
    static ggml_backend_t & backend_cpu(llama_context * c) { return c->backend_cpu; }
    static decltype(llama_context::set_n_threads_fns) & set_n_threads_fns(llama_context * c) { return c->set_n_threads_fns; }
    static ggml_threadpool_t & threadpool(llama_context * c) { return c->threadpool; }
    static ggml_threadpool_t & threadpool_batch(llama_context * c) { return c->threadpool_batch; }
    static int64_t & t_start_us(llama_context * c) { return c->t_start_us; }
    static int64_t & t_load_us(llama_context * c) { return c->t_load_us; }
    static int64_t & t_p_eval_us(llama_context * c) { return c->t_p_eval_us; }
    static int64_t & t_eval_us(llama_context * c) { return c->t_eval_us; }
    static int64_t & t_compute_start_us(llama_context * c) { return c->t_compute_start_us; }
    static int64_t & n_queued_tokens(llama_context * c) { return c->n_queued_tokens; }
    static int32_t & n_p_eval(llama_context * c) { return c->n_p_eval; }
    static int32_t & n_eval(llama_context * c) { return c->n_eval; }
    static bool & has_evaluated_once(llama_context * c) { return c->has_evaluated_once; }
    static std::vector<ggml_backend_ptr> & all_backends(llama_context * c) { return c->backends; }
    static ggml_abort_callback & abort_cb(llama_context * c) { return c->abort_callback; }
    static void *& abort_data(llama_context * c) { return c->abort_callback_data; }
    static auto & devices(llama_context * c) { return c->model.devices; }
    static ggml_opt_context_t & opt(llama_context * c) { return c->opt_ctx; }
    static void resolve_fused(llama_context * c, const llama_memory_context_i * m, uint32_t n) { c->resolve_fused_ops(m, n); }
    static bool out_reserve(llama_context * c, int32_t n) { return c->output_reserve(n) != 0; }
    static void realloc_cb(ggml_backend_sched_t s, ggml_cgraph * g, bool a, bool b, void * u) {
        llama_context::sched_graph_realloc(s, g, a, b, u);
    }
    static size_t state_write(llama_context * c, llama_io_write_i & io) { return c->state_write_data(io); }
    static size_t state_read (llama_context * c, llama_io_read_i  & io) { return c->state_read_data(io); }
    static size_t seq_write(llama_context * c, llama_io_write_i & io, llama_seq_id s, llama_state_seq_flags f) {
        return c->state_seq_write_data(io, s, f);
    }
    static size_t seq_read(llama_context * c, llama_io_read_i & io, llama_seq_id s, llama_state_seq_flags f) {
        return c->state_seq_read_data(io, s, f);
    }
    static auto & mem_storage(llama_context * c) { return c->mem_storage; }
    static void sched_set(llama_context * c, ggml_backend_sched_t s) { c->sched.reset(s); }
    static llama_memory_ptr & memory_ref(llama_context * c) { return c->memory; }
    static llama_moe_cache_ptr & moe_ref(llama_context * c) { return c->moe_cache; }
    static bool & sched_need_reserve(llama_context * c) { return c->sched_need_reserve; }
    static uint32_t & n_input_tensors(llama_context * c) { return c->n_input_tensors; }
    static auto & backend_buft(llama_context * c) { return c->backend_buft; }
    static int32_t & reallocs_same(llama_context * c) { return c->n_reallocs_same; }
    static int32_t & reallocs_diff(llama_context * c) { return c->n_reallocs_diff; }
    static ggml_backend_dev_t dev_layer(llama_context * c, int il) { return c->model.dev_layer(il); }
    static llm_graph_params make_graph_params(llama_context * c, llm_graph_result * r,
            const llama_ubatch & u, const llama_memory_context_i * m, llm_graph_type g) {
        return c->graph_params(r, u, m, g);
    }
    static bool native_copy_experts(ggml_backend_t be, const ggml_tensor * src, ggml_tensor * dst,
                                    ggml_cgraph * g, void * ud) {
        return llama_context::sched_copy_experts(be, src, dst, g, ud);
    }
};

static bool fg107_lazy_pp(llama_context * c) {
    auto & cp = fg107_context_access::cp(c);
    auto & hp = fg107_context_access::model(c).hparams;
    auto & be = fg107_context_access::backends(c);
    if (hp.no_alloc) return false;
    if (cp.training) return false;
    if (cp.pipeline_parallel) return false;
    if (!cp.causal_attn) return false;
    if (!cp.flash_attn) return false;
    if (cp.ctx_type != LLAMA_CONTEXT_TYPE_DEFAULT) return false;
    if (cp.n_seq_max != 1) return false;
    if (be.size() != 2) return false;
    if (ggml_backend_dev_type(ggml_backend_get_device(be[0])) != GGML_BACKEND_DEVICE_TYPE_GPU) return false;
    if (ggml_backend_dev_type(ggml_backend_get_device(be[1])) != GGML_BACKEND_DEVICE_TYPE_CPU) return false;
    const char * env = getenv("GGML_LAZY_PP_RESERVE");
    return env == nullptr || atoi(env) != 0;
}

extern "C" i64 fg107_ctx(i64 ctx, i64 field, i64 a) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto & cp = fg107_context_access::cp(c);
    auto & md = fg107_context_access::model(c);
    switch (field) {
        case 0:  return fg107_context_access::mem(c) != nullptr;
        case 1:  return fg107_context_access::sched_present(c);
        case 2:  return cp.n_ctx;
        case 3:  return cp.n_batch;
        case 4:  return cp.n_ubatch;
        case 5:  return cp.n_seq_max;
        case 6:  return md.vocab.n_tokens();
        case 7:  return md.hparams.n_embd;
        case 8:  return md.hparams.n_embd_out();
        case 9:  return md.hparams.n_cls_out;
        case 10: return cp.embeddings;
        case 11: return cp.embeddings_nextn;
        case 12: return cp.embeddings_nextn_masked;
        case 13: return cp.causal_attn;
        case 14: return cp.kv_unified;
        case 15: return cp.pipeline_parallel;
        case 16: return fg107_context_access::graph_reuse_disable(c);
        case 17: return cp.ctx_type == LLAMA_CONTEXT_TYPE_DEFAULT ? LLM_GRAPH_TYPE_DEFAULT : LLM_GRAPH_TYPE_DECODER_MTP;
        case 18: switch (cp.pooling_type) {
                     case LLAMA_POOLING_TYPE_NONE: return 0;
                     case LLAMA_POOLING_TYPE_MEAN: return 1;
                     case LLAMA_POOLING_TYPE_CLS:  return 2;
                     case LLAMA_POOLING_TYPE_LAST: return 3;
                     case LLAMA_POOLING_TYPE_RANK: return 4;
                     default:                      return 5;
                 }
        case 19: return md.arch == LLM_ARCH_T5;
        case 20: return fg107_context_access::graph_has_logits(c);
        case 21: return cp.n_outputs_max;
        case 22: return cp.n_outputs_max_per_seq;
        case 23: return !fg107_context_access::sampling(c).samplers.empty();
        case 24: return (i64) cp.embeddings_layer_inp.size();
        case 25: return fg107_context_access::n_outputs(c);
        case 26: return cp.no_perf;
        case 27: return cp.n_threads;
        case 28: return cp.n_threads_batch;
        case 29: return (i64) (a ? fg107_context_access::threadpool_batch(c) : fg107_context_access::threadpool(c));
        case 30: return fg107_lazy_pp(c);
        case 31: return LLAMA_MAX_SEQ;
        case 32: return std::numeric_limits<llama_pos>::max();
        case 33: return ggml_time_us();
        case 34: return c->graph_max_nodes((uint32_t) a);
        case 35: return (i64) (bool) cp.embeddings_layer_inp.at((size_t) a);
        case 36: return fg107_context_access::sampling(c).samplers.count((llama_seq_id) a) != 0;
        case 37: {
            if (F.sampler_counts.size() < (size_t) LLAMA_MAX_SEQ) F.sampler_counts.assign(LLAMA_MAX_SEQ, 0);
            return ++F.sampler_counts[(size_t) a];
        }
        case 39: return F.last_ret;
        default: GGML_ABORT("fg107_ctx: bad field");
    }
}

extern "C" i64 fg107_ctx_set(i64 ctx, i64 field, i64 v) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    switch (field) {
        case 0: fg107_context_access::n_outputs(c) = (uint32_t) v; return 0;
        case 1: fg107_context_access::note_reused(c); return 0;
        case 2: F.last_error = (int) v; return 0;
        case 3:
            for (auto & e : fg107_context_access::sampling(c).samplers) llama_sampler_backend_begin(e.second);
            return 0;
        case 4: F.last_ret = (int) v; return 0;
        case 5: F.sampler_counts.assign(LLAMA_MAX_SEQ, 0); return 0;
        default: GGML_ABORT("fg107_ctx_set: bad field");
    }
}

extern "C" i64 fg107_perf(i64 ctx, i64 op, i64 field, i64 value) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    if (op == 0) {
        switch (field) {
            case 0: return fg107_context_access::n_queued_tokens(c);
            case 1: return fg107_context_access::t_start_us(c);
            case 2: return fg107_context_access::t_compute_start_us(c);
            case 3: return fg107_context_access::n_eval(c);
            case 4: return fg107_context_access::t_eval_us(c);
            case 5: return fg107_context_access::n_p_eval(c);
            case 6: return fg107_context_access::t_p_eval_us(c);
            case 7: return fg107_context_access::has_evaluated_once(c);
            case 8: return fg107_context_access::t_load_us(c);
            case 9: return fg107_context_access::cp(c).no_perf;
            default: GGML_ABORT("fg107_perf: bad field");
        }
    }
    switch (field) {
        case 0: fg107_context_access::n_queued_tokens(c) = value; return 0;
        case 1: fg107_context_access::t_start_us(c) = value; return 0;
        case 2: fg107_context_access::t_compute_start_us(c) = value; return 0;
        case 3: fg107_context_access::n_eval(c) = (int32_t) value; return 0;
        case 4: fg107_context_access::t_eval_us(c) = value; return 0;
        case 5: fg107_context_access::n_p_eval(c) = (int32_t) value; return 0;
        case 6: fg107_context_access::t_p_eval_us(c) = value; return 0;
        case 7: fg107_context_access::has_evaluated_once(c) = value != 0; return 0;
        case 8: fg107_context_access::t_load_us(c) = value; return 0;
        default: GGML_ABORT("fg107_perf: bad field");
    }
}

extern "C" i64 fg107_log(i64 ctx, i64 site, i64 a, i64 b) {
    (void) ctx;
    switch (site) {
        case 1:  LLAMA_LOG_ERROR("%s: graph compute failed with status %d\n", __func__, (int) a); break;
        case 2:  LLAMA_LOG_ERROR("%s: failed to apply memory context\n", __func__); break;
        case 3:  LLAMA_LOG_ERROR("%s: failed to initialize graph\n", __func__); break;
        case 4:  LLAMA_LOG_ERROR("%s: failed to allocate graph\n", __func__); break;
        case 5:  LLAMA_LOG_ERROR("%s: failed to compute graph, status %d\n", __func__, (int) a); break;
        case 6:  LLAMA_LOG_ERROR("%s: failed to allocate output buffer of %lld bytes\n", __func__, (long long) a); break;
        case 7:  LLAMA_LOG_ERROR("%s: failed to prepare memory update\n", __func__); break;
        case 8:  LLAMA_LOG_ERROR("%s: failed to apply memory update\n", __func__); break;
        case 9:  LLAMA_LOG_ERROR("%s: failed to reserve graph after the memory update\n", __func__); break;
        case 10: LLAMA_LOG_ERROR("%s: n_tokens == 0\n", __func__); break;
        case 11: LLAMA_LOG_ERROR("%s: embd row width %lld does not match %lld\n", __func__, (long long) a, (long long) b); break;
        case 12: LLAMA_LOG_ERROR("%s: backend sampling supports at most n_outputs_max_per_seq outputs (seq_id %lld had %lld)\n", __func__, (long long) a, (long long) b); break;
        case 13: LLAMA_LOG_ERROR("%s: failed to initialize batch\n", __func__); break;
        case 14: LLAMA_LOG_ERROR("%s: pooled embedding requires all tokens to be output (%lld vs %lld)\n", __func__, (long long) a, (long long) b); break;
        case 15: LLAMA_LOG_ERROR("%s: unexpected memory context status: %lld\n", __func__, (long long) a); break;
        case 16: LLAMA_LOG_DEBUG("%s: retrying batch size %lld after cache optimization\n", __func__, (long long) a); break;
        case 17: LLAMA_LOG_WARN("%s: failed to find a memory slot for batch of size %lld\n", __func__, (long long) a); break;
        case 18: LLAMA_LOG_ERROR("%s: compute failed while preparing batch of size %lld\n", __func__, (long long) a); break;
        case 19: LLAMA_LOG_ERROR("%s: could not reserve space for batch with %lld outputs\n", __func__, (long long) a); break;
        case 20: LLAMA_LOG_WARN("%s: removing memory module entries for seq_id = %lld, pos = [%lld, +inf)\n", __func__, (long long) a, (long long) b); break;
        case 30: LLAMA_LOG_ERROR("%s: error getting state size\n", __func__); break;
        case 31: LLAMA_LOG_ERROR("%s: error saving state\n", __func__); break;
        case 32: LLAMA_LOG_ERROR("%s: error loading state\n", __func__); break;
        default: GGML_ABORT("fg107_log: bad site");
    }
    return 0;
}

extern "C" i64 fg107_batch(i64 b, i64 field, i64 i, i64 s) {
    auto * x = (const llama_batch_ext *) (intptr_t) b;
    switch (field) {
        case 0: return (i64) x->tokens.size();
        case 1: return (i64) x->n_embd;
        case 2: return (i64) x->n_embd_inp;
        case 3: return x->tokens[(size_t) i].output;
        case 4: return (i64) x->tokens[(size_t) i].seq_ids.size();
        case 5: return *std::next(x->tokens[(size_t) i].seq_ids.begin(), (long) s);
        default: GGML_ABORT("fg107_batch: bad field");
    }
}

extern "C" i64 fg107_ub(i64 u, i64 field, i64 i, i64 s) {
    auto * x = (const llama_ubatch *) (intptr_t) u;
    switch (field) {
        case 0: return x->n_tokens;
        case 1: return x->output[i];
        case 2: return x->n_seq_id[i];
        case 3: return x->seq_id[i][s];
        case 4: return x->pos[i];
        case 5: return x->n_seqs_unq;
        case 6: return x->seq_id_unq[s];
        case 7: return x->seq_idx[s];
        case 8: return (i64) x->data->batch_idxs.size();
        case 9: return x->data->batch_idxs[(size_t) i];
        default: GGML_ABORT("fg107_ub: bad field");
    }
}

extern "C" i64 fg107_balloc(i64 ctx, i64 op, i64 a, i64 b) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto * ba = fg107_context_access::balloc(c);
    switch (op) {
        case 0: return ba->init(*(const llama_batch_ext *) (intptr_t) a, fg107_context_access::model(c).vocab, b != 0);
        case 1: return ba->get_n_tokens();
        case 2: return ba->get_n_outputs();
        case 3: return (i64) ba->get_out_ids().size();
        case 4: return ba->get_out_ids()[(size_t) a];
        case 5: std::swap(ba->get_out_ids()[(size_t) a], ba->get_out_ids()[(size_t) b]); return 0;
        default: GGML_ABORT("fg107_balloc: bad op");
    }
}

extern "C" i64 fg107_mem(i64 ctx, i64 op, i64 a, i64 b, i64 d) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto * m = fg107_context_access::mem(c);
    if (!m) return 0;
    switch (op) {
        case 0: F.mctx = m->init_batch(*fg107_context_access::balloc(c), (uint32_t) a, b != 0); return (i64) (intptr_t) F.mctx.get();
        case 1: F.mctx = m->init_full(); return (i64) (intptr_t) F.mctx.get();
        case 2: F.mctx = m->init_update(c, a != 0); return (i64) (intptr_t) F.mctx.get();
        case 3: return m->seq_rm((llama_seq_id) a, (llama_pos) b, (llama_pos) d);
        default: GGML_ABORT("fg107_mem: bad op");
    }
}

extern "C" i64 fg107_mctx(i64 m, i64 op, i64 a) {
    (void) a;
    auto * x = (llama_memory_context_i *) (intptr_t) m;
    switch (op) {
        case 0: return (i64) x->get_status();
        case 1: return x->apply();
        case 2: return x->next();
        case 3: return (i64) (intptr_t) &x->get_ubatch();
        default: GGML_ABORT("fg107_mctx: bad op");
    }
}

extern "C" i64 fg107_res_prev(i64 ctx) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto & slot = fg107_context_access::gf_prev(c)[fg107_context_access::n_outputs(c) > 0];
    if (!slot) slot.reset(new llm_graph_result(fg107_context_access::gf_reserve(c)->get_max_nodes()));
    return (i64) (intptr_t) slot.get();
}
extern "C" i64 fg107_res_prev_active(i64 ctx) {
    return (i64) (intptr_t) fg107_context_access::prev_active(fr(ctx).lctx);
}
extern "C" i64 fg107_res_set_prev_active(i64 ctx, i64 r) {
    fg107_context_access::prev_active(fr(ctx).lctx) = (llm_graph_result *) (intptr_t) r;
    return 0;
}
extern "C" i64 fg107_res_reset_all_prev(i64 ctx) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    for (auto & r : fg107_context_access::gf_prev(c)) if (r) r->reset();
    fg107_context_access::prev_active(c) = nullptr;
    return 0;
}

extern "C" i64 fg107_res(i64 ctx, i64 r, i64 op, i64 a) {
    auto & F = fr(ctx);
    auto * x = (llm_graph_result *) (intptr_t) r;
    switch (op) {
        case 0: return (i64) (intptr_t) x->get_gf();
        case 1: return F.gparams && x->can_reuse(*F.gparams);
        case 2: x->reset(); return 0;
        case 3: x->set_inputs((const llama_ubatch *) (intptr_t) a); return 0;
        case 4: return (i64) (intptr_t) x->get_logits();
        case 5: return (i64) (intptr_t) x->get_embd();
        case 6: return (i64) (intptr_t) x->get_embd_pooled();
        case 7: return (i64) (intptr_t) x->get_h_nextn();
        case 8: return (i64) (intptr_t) x->get_layer_inp((int) a);
        case 9: return (i64) x->t_sampled.size();
        default: GGML_ABORT("fg107_res: bad op");
    }
}

extern "C" i64 fg107_res_sampling_tensor(i64 r, i64 kind, i64 i) {
    auto * x = (llm_graph_result *) (intptr_t) r;
    const std::vector<ggml_tensor *> * v = nullptr;
    switch (kind) {
        case 0: v = &x->t_sampled; break;
        case 1: v = &x->t_sampled_logits; break;
        case 2: v = &x->t_sampled_probs; break;
        case 3: v = &x->t_candidates; break;
        default: GGML_ABORT("fg107_res_sampling_tensor: bad kind");
    }
    if (i < 0) return (i64) v->size();
    if ((size_t) i >= v->size()) return 0;
    return (i64) (intptr_t) (*v)[(size_t) i];
}

extern "C" i64 fg107_graph_params(i64 ctx, i64 r, i64 u, i64 m, i64 gtype) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    F.gparams.reset(new llm_graph_params(fg107_context_access::make_graph_params(c,
                           (llm_graph_result *) (intptr_t) r,
                           *(const llama_ubatch *) (intptr_t) u,
                           (const llama_memory_context_i *) (intptr_t) m,
                           (llm_graph_type) gtype)));
    return (i64) (intptr_t) F.gparams.get();
}
extern "C" i64 fg107_model_build_graph(i64 ctx, i64 gp) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    return (i64) (intptr_t) fg107_context_access::model(c).build_graph(*(const llm_graph_params *) (intptr_t) gp);
}
extern "C" i64 fg107_graph_reserve(i64 ctx, i64 t, i64 s, i64 o, i64 m, i64 lazy) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    return (i64) (intptr_t) c->graph_reserve((uint32_t) t, (uint32_t) s, (uint32_t) o,
                                             (const llama_memory_context_i *) (intptr_t) m, lazy != 0);
}

extern "C" i64 fg107_graph(i64 g, i64 op, i64 a) {
    auto * gf = (ggml_cgraph *) (intptr_t) g;
    switch (op) {
        case 0: return ggml_graph_n_nodes(gf);
        case 1: return (i64) (intptr_t) ggml_graph_node(gf, (int) a);
        default: GGML_ABORT("fg107_graph: bad op");
    }
}

extern "C" i64 fg107_sched(i64 ctx, i64 op, i64 a, i64 b, i64 d) {
    (void) b; (void) d;
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto * sc = fg107_context_access::sched(c);
    switch (op) {
        case 0: ggml_backend_sched_reset(sc); return 0;
        case 1: {
            auto & cp = fg107_context_access::cp(c);
            ggml_backend_sched_set_eval_callback(sc, cp.cb_eval, cp.cb_eval_user_data);
            return 0;
        }
        case 2: return ggml_backend_sched_alloc_graph(sc, (ggml_cgraph *) (intptr_t) a);
        case 3: ggml_backend_sched_synchronize(sc); return 0;
        case 4: return (i64) (intptr_t) ggml_backend_sched_get_tensor_backend(sc, (ggml_tensor *) (intptr_t) a);
        case 5: return (i64) fg107_context_access::backends(c).size();
        case 6: return ggml_backend_sched_graph_compute_async(sc, (ggml_cgraph *) (intptr_t) a);
        case 7: c->sched_reserve(); return 0;
        case 8: fg107_context_access::copy_experts(c).reset(); return 0;
        case 9: {
            auto & bc = fg107_context_access::backend_cpu(c);
            if (bc) {
                auto * reg = ggml_backend_dev_backend_reg(ggml_backend_get_device(bc));
                auto * fn = (void (*)(ggml_backend_t, ggml_threadpool_t))
                        ggml_backend_reg_get_proc_address(reg, "ggml_backend_cpu_set_threadpool");
                if (fn) fn(bc, (ggml_threadpool_t) (intptr_t) a);
            }
            return 0;
        }
        case 10:
            for (auto & e : fg107_context_access::set_n_threads_fns(c)) e.second(e.first, (int) a);
            return 0;
        case 11: {
            auto & cp = fg107_context_access::cp(c);
            return a ? cp.n_threads_batch : cp.n_threads;
        }
        case 12: return (i64) (intptr_t) (a ? fg107_context_access::threadpool_batch(c)
                                           : fg107_context_access::threadpool(c));
        case 13: return ggml_backend_sched_get_buffer_size(sc, (ggml_backend_t) (intptr_t) a);
        case 14: {
            auto & be = fg107_context_access::backends(c);
            auto & sz = fg107_context_access::buf_exp_size(c);
            for (size_t i = 0; i < be.size(); ++i) {
                if (be[i] == (ggml_backend_t) (intptr_t) a) sz[i] = ggml_backend_sched_get_buffer_size(sc, be[i]);
            }
            return 0;
        }
        case 15: return (i64) (intptr_t) fg107_context_access::backends(c).at((size_t) a);
        default: GGML_ABORT("fg107_sched: bad op");
    }
}

extern "C" i64 fg107_tensor(i64 t, i64 field, i64 i) {
    auto * x = (const ggml_tensor *) (intptr_t) t;
    switch (field) {
        case 0: return x->op;
        case 1: return (i64) (intptr_t) x->src[i];
        case 2: return x->ne[i];
        case 3: return (i64) x->nb[i];
        case 4: return ggml_nelements(x);
        case 5: return (i64) ggml_nbytes(x);
        case 6: return ggml_is_contiguous(x);
        case 7: return (i64) (intptr_t) x->data;
        default: GGML_ABORT("fg107_tensor: bad field");
    }
}
extern "C" i64 fg107_tensor_get_async(i64 be, i64 t, i64 dst, i64 off, i64 size) {
    ggml_backend_tensor_get_async((ggml_backend_t) (intptr_t) be, (const ggml_tensor *) (intptr_t) t,
                                  (void *) (intptr_t) dst, (size_t) off, (size_t) size);
    return 0;
}
extern "C" i64 fg107_tensor_set_async(i64 be, i64 t, i64 src, i64 off, i64 size) {
    ggml_backend_tensor_set_async((ggml_backend_t) (intptr_t) be, (ggml_tensor *) (intptr_t) t,
                                  (const void *) (intptr_t) src, (size_t) off, (size_t) size);
    return 0;
}
extern "C" i64 fg107_backend_synchronize(i64 be) {
    ggml_backend_synchronize((ggml_backend_t) (intptr_t) be);
    return 0;
}

extern "C" i64 fg107_moe(i64 ctx, i64 op, i64 a, i64 b, i64 d, i64 e, i64 f) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto * mc = fg107_context_access::moe(c);
    switch (op) {
        case 0: return mc != nullptr;
        case 1: return mc && mc->copy((ggml_backend_t) (intptr_t) a, (const ggml_tensor *) (intptr_t) b,
                                      (ggml_tensor *) (intptr_t) d, (ggml_cgraph *) (intptr_t) e);
        case 2: return mc ? mc->copy_experts((ggml_backend_t) (intptr_t) a, (const ggml_tensor *) (intptr_t) b,
                                             (ggml_tensor *) (intptr_t) d, (int64_t) e, (int64_t) f) : 0;
        default: GGML_ABORT("fg107_moe: bad op");
    }
}

extern "C" i64 fg107_op_mul_mat_id(void) { return GGML_OP_MUL_MAT_ID; }

static float * fg107_view_f32(llama_context * c, i64 which, i64 il) {
    switch (which) {
        case 0: return fg107_context_access::logits(c).data;
        case 1: return fg107_context_access::embd(c).data;
        case 2: return fg107_context_access::embd_nextn(c).data;
        case 3: return fg107_context_access::layer_inp(c).at((size_t) il).data;
        case 4: return fg107_context_access::sampling(c).logits.data;
        case 5: return fg107_context_access::sampling(c).probs.data;
        default: return nullptr;
    }
}

extern "C" i64 fg107_out(i64 ctx, i64 op, i64 a, i64 b, i64 d) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto & cp = fg107_context_access::cp(c);
    auto & smp = fg107_context_access::sampling(c);
    switch (op) {
        case 0: return fg107_context_access::buf_output(c) ? (i64) ggml_backend_buffer_get_size(fg107_context_access::buf_output(c).get()) : 0;
        case 1: {
            const size_t new_size = (size_t) a;
            auto & buf = fg107_context_access::buf_output(c);
            const size_t prev = buf ? ggml_backend_buffer_get_size(buf.get()) : 0;
            if (!buf || prev < new_size) {
                if (buf) {
                    c->synchronize();
                    buf = nullptr;
                    fg107_context_access::logits(c) = {nullptr, 0};
                    fg107_context_access::embd(c) = {nullptr, 0};
                    fg107_context_access::embd_nextn(c) = {nullptr, 0};
                    for (auto & v : fg107_context_access::layer_inp(c)) v = {nullptr, 0};
                }
                auto * buft = ggml_backend_cpu_buffer_type();
                auto * dev = fg107_context_access::model(c).dev_output();
                auto * host = dev ? ggml_backend_dev_host_buffer_type(dev) : nullptr;
                if (host) buft = host;
                buf.reset(ggml_backend_buft_alloc_buffer(buft, new_size));
                if (!buf) return 0;
                ggml_backend_buffer_clear(buf.get(), 0);
            }
            return 1;
        }
        case 2: return (i64) (intptr_t) ggml_backend_buffer_get_base(fg107_context_access::buf_output(c).get());
        case 3: return (i64) (intptr_t) fg107_context_access::logits(c).data;
        case 4: return (i64) fg107_context_access::logits(c).size;
        case 5: return (i64) (intptr_t) fg107_context_access::embd(c).data;
        case 6: return (i64) fg107_context_access::embd(c).size;
        case 7: return (i64) (intptr_t) fg107_context_access::embd_nextn(c).data;
        case 8: return (i64) fg107_context_access::embd_nextn(c).size;
        case 9: return (i64) (intptr_t) fg107_context_access::layer_inp(c).at((size_t) a).data;
        case 10: return (i64) fg107_context_access::layer_inp(c).at((size_t) a).size;
        case 11: return fg107_context_access::layer_inp(c).at((size_t) a).has_data();
        case 12: return (i64) fg107_context_access::embd_seq(c).size();
        case 13: fg107_context_access::embd_seq(c).clear(); return 0;
        case 14: fg107_context_access::embd_seq(c)[(llama_seq_id) a].resize((size_t) b); return 0;
        case 15: return (i64) (intptr_t) fg107_context_access::embd_seq(c)[(llama_seq_id) a].data();
        case 16: {
            smp.logits_count.resize((size_t) a);
            smp.probs_count.resize((size_t) a);
            smp.candidates_count.resize((size_t) a);
            std::fill(smp.logits_count.begin(), smp.logits_count.end(), 0);
            std::fill(smp.probs_count.begin(), smp.probs_count.end(), 0);
            std::fill(smp.candidates_count.begin(), smp.candidates_count.end(), 0);
            if (smp.sampled.data) std::fill_n(smp.sampled.data, smp.sampled.size, LLAMA_TOKEN_NULL);
            return 0;
        }
        case 17:
            smp.logits = {nullptr, 0}; smp.probs = {nullptr, 0};
            smp.sampled = {nullptr, 0}; smp.candidates = {nullptr, 0};
            smp.logits_count.clear(); smp.probs_count.clear(); smp.candidates_count.clear();
            return 0;
        case 18: switch (a) {
                     case 0: return (i64) (intptr_t) smp.sampled.data;
                     case 1: return (i64) (intptr_t) smp.logits.data;
                     case 2: return (i64) (intptr_t) smp.probs.data;
                     case 3: return (i64) (intptr_t) smp.candidates.data;
                     default: GGML_ABORT("fg107_out: bad sampling kind");
                 }
        case 19: switch (a) {
                     case 0: return (i64) smp.sampled.size;
                     case 1: return (i64) smp.logits.size;
                     case 2: return (i64) smp.probs.size;
                     case 3: return (i64) smp.candidates.size;
                     default: GGML_ABORT("fg107_out: bad sampling kind");
                 }
        case 20: switch (a) {
                     case 0: return (i64) smp.sampled.size;
                     case 1: return (i64) smp.logits_count.size();
                     case 2: return (i64) smp.probs_count.size();
                     case 3: return (i64) smp.candidates_count.size();
                     default: GGML_ABORT("fg107_out: bad sampling kind");
                 }
        case 21: switch (a) {
                     case 0: smp.sampled.data[(size_t) b] = (llama_token) d; return 0;
                     case 1: smp.logits_count[(size_t) b] = (uint32_t) d; return 0;
                     case 2: smp.probs_count[(size_t) b] = (uint32_t) d; return 0;
                     case 3: smp.candidates_count[(size_t) b] = (uint32_t) d; return 0;
                     default: GGML_ABORT("fg107_out: bad sampling kind");
                 }
        case 22: return (a == 1 || a == 2) ? (i64) sizeof(float) : (i64) sizeof(llama_token);
        case 23: switch (a) {
                     case 0: std::swap(smp.sampled.data[(size_t) b], smp.sampled.data[(size_t) d]); return 0;
                     case 1: std::swap(smp.logits_count[(size_t) b], smp.logits_count[(size_t) d]); return 0;
                     case 2: std::swap(smp.probs_count[(size_t) b], smp.probs_count[(size_t) d]); return 0;
                     case 3: std::swap(smp.candidates_count[(size_t) b], smp.candidates_count[(size_t) d]); return 0;
                     default: GGML_ABORT("fg107_out: bad sampling kind");
                 }
        default: GGML_ABORT("fg107_out: bad op");
    }
}

extern "C" i64 fg107_out_bind(i64 ctx, i64 which, i64 il, i64 off, i64 count) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto & buf = fg107_context_access::buf_output(c);
    uint8_t * base = buf ? (uint8_t *) ggml_backend_buffer_get_base(buf.get()) : nullptr;
    void * p = count > 0 ? (void *) (base + (size_t) off) : nullptr;
    switch (which) {
        case 0: fg107_context_access::logits(c) = {(float *) p, (size_t) count}; return 0;
        case 1: fg107_context_access::embd(c) = {(float *) p, (size_t) count}; return 0;
        case 2: fg107_context_access::embd_nextn(c) = {(float *) p, (size_t) count}; return 0;
        case 3: fg107_context_access::layer_inp(c)[(size_t) il] = {(float *) p, (size_t) count}; return 0;
        case 4: fg107_context_access::sampling(c).logits = {(float *) p, (size_t) count}; return 0;
        case 5: fg107_context_access::sampling(c).probs = {(float *) p, (size_t) count}; return 0;
        case 6: fg107_context_access::sampling(c).sampled = {(llama_token *) p, (size_t) count}; return 0;
        case 7: fg107_context_access::sampling(c).candidates = {(llama_token *) p, (size_t) count}; return 0;
        default: GGML_ABORT("fg107_out_bind: bad which");
    }
}

extern "C" i64 fg107_out_view_swap(i64 ctx, i64 which, i64 il, i64 i, i64 j) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    if (which == 6) {
        auto & v = fg107_context_access::sampling(c).candidates;
        std::swap(v.data[(size_t) i], v.data[(size_t) j]);
        return 0;
    }
    float * p = fg107_view_f32(c, which, il);
    if (!p) return 0;
    std::swap(p[(size_t) i], p[(size_t) j]);
    return 0;
}

extern "C" i64 fg107_exec(i64 ctx, i64 op, i64 a, i64 b, i64 d) {
    (void) d;
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto & ids = fg107_context_access::output_ids(c);
    auto & sw  = fg107_context_access::swaps(c);
    auto & ebi = fg107_context_access::embd_batch_idxs(c);
    auto & ce  = fg107_context_access::copy_experts(c);
    switch (op) {
        case 0: if (ids.empty()) ids.resize((size_t) a); return 0;
        case 1: return (i64) ids.size();
        case 2: return ids[(size_t) a];
        case 3: ids[(size_t) a] = (int32_t) b; return 0;
        case 4: std::fill(ids.begin(), ids.end(), (int32_t) a); return 0;
        case 5: return (i64) sw.size();
        case 6: return b == 0 ? (i64) sw[(size_t) a].i0 : (i64) sw[(size_t) a].i1;
        case 7: sw.push_back({(uint32_t) a, (uint32_t) b}); return 0;
        case 8: sw.clear(); return 0;
        case 9: return (i64) ebi.size();
        case 10: return ebi[(size_t) a];
        case 11: {
            auto * u = (const llama_ubatch *) (intptr_t) a;
            ebi.insert(ebi.end(), u->data->batch_idxs.begin(), u->data->batch_idxs.end());
            return 0;
        }
        case 12: ebi.clear(); return 0;
        case 13: std::swap(ebi[(size_t) a], ebi[(size_t) b]); return 0;
        case 14: return (i64) (intptr_t) ce.ids;
        case 15: ce.ids = (const ggml_tensor *) (intptr_t) a; return 0;
        case 16: ce.ids_data.resize((size_t) a); return 0;
        case 17: return (i64) (intptr_t) ce.ids_data.data();
        case 18: return ce.ids_data[(size_t) a];
        case 19: return (i64) ce.used.size();
        case 20: ce.used.assign((size_t) a, false); return 0;
        case 21: return ce.used[(size_t) a];
        case 22: ce.used[(size_t) a] = b != 0; return 0;
        default: GGML_ABORT("fg107_exec: bad op");
    }
}
extern "C" i64 fg107_exec_copy_id_at(i64 ctx, i64 i) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    return fg107_context_access::copy_experts(c).ids_data[(size_t) i];
}

static bool fg107_sched_copy_experts_cb(ggml_backend_t be, const ggml_tensor * src, ggml_tensor * dst,
                                        ggml_cgraph * g, void * ud) {
#ifdef FG107_NATIVE_ROLLBACK
    // Explicit rollback build: use the original native callback. This is the
    // only allowed non-Goose path and must be selected at configure time.
    return fg107_context_access::native_copy_experts(be, src, dst, g, ud);
#else
    // Strong export: absence is a link error, not a silent `return false`.
    return fg107_goose_sched_copy_experts((i64) (intptr_t) ud, (i64) (intptr_t) be,
                                          (i64) (intptr_t) src, (i64) (intptr_t) dst,
                                          (i64) (intptr_t) g) != 0;
#endif
}

extern "C" i64 fg107_sched_set_copy_callback(i64 lctx_raw) {
    auto * c = (llama_context *) (intptr_t) lctx_raw;
    GGML_ASSERT(c != nullptr && fg107_context_access::sched(c) != nullptr);
    ggml_backend_sched_set_copy_callback(fg107_context_access::sched(c), fg107_sched_copy_experts_cb, c);
    return 0;
}

extern "C" i64 fg107_ctx_set_abort_callback(i64 lctx_raw, i64 cb, i64 data) {
    auto * c = (llama_context *) (intptr_t) lctx_raw;
    fg107_context_access::abort_cb(c) = (ggml_abort_callback) (intptr_t) cb;
    fg107_context_access::abort_data(c) = (void *) (intptr_t) data;
    for (auto & b : fg107_context_access::all_backends(c)) {
        auto * reg = ggml_backend_dev_backend_reg(ggml_backend_get_device(b.get()));
        if (!reg) continue;
        auto * fn = (ggml_backend_set_abort_callback_t)
                ggml_backend_reg_get_proc_address(reg, "ggml_backend_set_abort_callback");
        if (fn) fn(b.get(), fg107_context_access::abort_cb(c), fg107_context_access::abort_data(c));
    }
    return 0;
}

// ===========================================================================
// resource lifecycle / checkpoint services (context-resource.goose)
// ===========================================================================
#include "llama-io.h"

static size_t fg107_graph_n_input_tensors(ggml_cgraph * gf) {
    std::map<const ggml_tensor *, std::vector<const ggml_tensor *>> users;
    for (int i = 0; i < ggml_graph_n_nodes(gf); ++i) {
        const ggml_tensor * node = ggml_graph_node(gf, i);
        for (int j = 0; j < GGML_MAX_SRC; ++j) {
            const ggml_tensor * src = node->src[j];
            if (!src) break;
            if (src->flags & GGML_TENSOR_FLAG_INPUT) users[src].push_back(node);
        }
    }
    return users.size();
}

extern "C" i64 fg107_r_state(i64 ctx, i64 op, i64 a, i64 b, i64 d);

extern "C" i64 fg107_rq(i64 ctx, i64 field, i64 a) {
    auto * c = fr(ctx).lctx;
    auto & cp = fg107_context_access::cp(c);
    auto & md = fg107_context_access::model(c);
    switch (field) {
        case 0: return md.hparams.vocab_only;
        case 1: return (i64) fg107_context_access::devices(c).size();
        case 2: return fg107_context_access::sched_need_reserve(c);
        case 3: return cp.training;
        case 4: return md.hparams.no_alloc;
        case 5: return md.n_devices();
        case 6: return md.n_gpu_layers();
        case 7: return md.hparams.n_layer_all;
        case 8: return md.split_mode() == LLAMA_SPLIT_MODE_LAYER;
        case 9: return cp.offload_kqv;
        case 10: return md.has_tensor_overrides();
        case 11: return cp.moe_cache_size;
        case 12: return fg107_context_access::n_input_tensors(c);
        case 13: return fg107_context_access::graph_has_logits(c);
        case 14: return (i64) fg107_context_access::backends(c).size();
        case 15: return (i64) md.arch;
        case 16: return ggml_is_quantized((ggml_type) a);
        case 17: return cp.flash_attn;
        case 18: return md.arch == LLM_ARCH_QWEN35MOE;
        case 19: return md.arch == LLM_ARCH_KIMI_LINEAR || md.arch == LLM_ARCH_MINIMAX_01;
        case 20: return md.hparams.n_layer();
        default: GGML_ABORT("fg107_rq: bad field");
    }
}
extern "C" i64 fg107_rq_set(i64 ctx, i64 field, i64 v) {
    auto * c = fr(ctx).lctx;
    switch (field) {
        case 0: fg107_context_access::sched_need_reserve(c) = v != 0; return 0;
        case 1: fg107_context_access::graph_has_logits(c) = v != 0; return 0;
        case 2: fg107_context_access::cp(c).pipeline_parallel = v != 0; return 0;
        case 3: fg107_context_access::n_input_tensors(c) = (uint32_t) v; return 0;
        default: GGML_ABORT("fg107_rq_set: bad field");
    }
}

extern "C" i64 fg107_r(i64 ctx, i64 op, i64 a, i64 b, i64 d) {
    auto & F = fr(ctx);
    auto * c = F.lctx;
    auto & cp = fg107_context_access::cp(c);
    auto & md = fg107_context_access::model(c);
    (void) d;
    switch (op) {
        case 0: return (i64) (intptr_t) fg107_context_access::devices(c).at((size_t) a).dev;
        case 1: return (i64) (intptr_t) ggml_backend_dev_init((ggml_backend_dev_t) (intptr_t) a, nullptr);
        case 2: fg107_context_access::all_backends(c).emplace_back((ggml_backend_t) (intptr_t) a); return 0;
        case 3: return (i64) ggml_backend_dev_count();
        case 4: return (i64) (intptr_t) ggml_backend_dev_get((size_t) a);
        case 5: return ggml_backend_dev_type((ggml_backend_dev_t) (intptr_t) a) == GGML_BACKEND_DEVICE_TYPE_ACCEL;
        case 6: {
            ggml_backend_t be = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, nullptr);
            fg107_context_access::backend_cpu(c) = be;
            return (i64) (intptr_t) be;
        }
        case 7:
            for (auto & be : fg107_context_access::all_backends(c)) {
                auto * dev = ggml_backend_get_device(be.get());
                auto * reg = dev ? ggml_backend_dev_backend_reg(dev) : nullptr;
                if (!reg) continue;
                auto * fn = (ggml_backend_set_n_threads_t)
                        ggml_backend_reg_get_proc_address(reg, "ggml_backend_set_n_threads");
                if (fn) fg107_context_access::set_n_threads_fns(c).emplace_back(be.get(), fn);
            }
            return 0;
        case 8: c->set_abort_callback((bool (*)(void *)) F.abort_cb, F.abort_data); return 0;
        case 9: return fg107_context_access::out_reserve(c, (int32_t) a);
        case 10: {
            llama_memory_params p = {};
            p.type_k = (ggml_type) F.type_k;
            p.type_v = (ggml_type) F.type_v;
            p.swa_full = F.swa_full;
            p.ctx_type = cp.ctx_type;
            p.mem_other = llama_get_memory(cp.ctx_other);
            fg107_context_access::memory_ref(c).reset(md.create_memory(p, cp));
            return (i64) (intptr_t) fg107_context_access::mem(c);
        }
        case 11: {
            auto & be = fg107_context_access::backends(c);
            auto & buft = fg107_context_access::buf_exp_size(c);
            be.clear(); buft.clear();
            auto & bufts = fg107_context_access::backend_buft(c);
            bufts.clear();
            for (auto & b : fg107_context_access::all_backends(c)) {
                auto * bt = ggml_backend_get_default_buffer_type(b.get());
                auto type = ggml_backend_dev_type(ggml_backend_get_device(b.get()));
                if (type == GGML_BACKEND_DEVICE_TYPE_CPU && !fg107_context_access::devices(c).empty()) {
                    auto * host = ggml_backend_dev_host_buffer_type(fg107_context_access::devices(c)[0].dev);
                    if (host) bt = host;
                }
                bufts.push_back(bt);
                be.push_back(b.get());
                buft.push_back(0);
            }
            return 0;
        }
        case 12: {
            bool pp = md.n_devices() > 1 && md.n_gpu_layers() > md.hparams.n_layer_all &&
                      md.split_mode() == LLAMA_SPLIT_MODE_LAYER && cp.offload_kqv &&
                      !md.has_tensor_overrides() && cp.moe_cache_size == 0;
            if (pp) {
                for (auto & b : fg107_context_access::all_backends(c)) {
                    if (ggml_backend_dev_type(ggml_backend_get_device(b.get())) == GGML_BACKEND_DEVICE_TYPE_CPU) continue;
                    ggml_backend_dev_props props;
                    ggml_backend_dev_get_props(ggml_backend_get_device(b.get()), &props);
                    if (!props.caps.async || !props.caps.events) { pp = false; break; }
                }
            }
            cp.pipeline_parallel = pp;
            return pp;
        }
        case 13:
            if (cp.moe_cache_size > 0) {
                fg107_context_access::moe_ref(c) = std::make_unique<llama_moe_cache>(
                        md, fg107_context_access::backends(c), fg107_context_access::backend_buft(c), cp.moe_cache_size);
            }
            return 0;
        case 14:
            return !(!cp.flash_attn && ggml_is_quantized((ggml_type) F.type_v));
        case 15: {
            auto & be = fg107_context_access::backends(c);
            if (md.hparams.no_alloc || cp.training || !cp.causal_attn || !cp.flash_attn ||
                cp.ctx_type != LLAMA_CONTEXT_TYPE_DEFAULT || cp.n_seq_max != 1 ||
                cp.pipeline_parallel || !cp.op_offload || cp.moe_cache_size != 0 ||
                be.size() != 2) return 0;
            if (ggml_backend_dev_type(ggml_backend_get_device(be[0])) != GGML_BACKEND_DEVICE_TYPE_GPU) return 0;
            if (ggml_backend_dev_type(ggml_backend_get_device(be[1])) != GGML_BACKEND_DEVICE_TYPE_CPU) return 0;
            ggml_backend_dev_props props;
            ggml_backend_dev_get_props(ggml_backend_get_device(be[0]), &props);
            return props.caps.host_buffer;
        }
        case 16: {
            auto & be = fg107_context_access::backends(c);
            if (be.empty()) return 0;
            return fg107_compat_pin_experts(&md,
                    ggml_backend_dev_backend_reg(ggml_backend_get_device(be[0])));
        }
        case 17: {
            const int n_vocab = md.vocab.n_tokens();
            auto & ids = fg107_context_access::sampling(c).token_ids_full_vocab;
            ids.resize((size_t) n_vocab);
            for (int i = 0; i < n_vocab; ++i) ids[(size_t) i] = i;
            return 0;
        }
        case 18: fg107_context_access::gf_reserve(c).reset(new llm_graph_result((size_t) a)); return 0;
        case 19: return fg107_goose_resolve_fused(ctx, a, b);
        case 23: return (i64) fg107_graph_n_input_tensors((ggml_cgraph *) (intptr_t) a);
        case 25: return (i64) ggml_backend_sched_get_buffer_size(fg107_context_access::sched(c), (ggml_backend_t) (intptr_t) a);
        case 26: fg107_context_access::buf_exp_size(c).at((size_t) a) = (size_t) b; return 0;
        case 27: {
            auto & be = fg107_context_access::backends(c);
            auto & buft = fg107_context_access::backend_buft(c);
            fg107_context_access::sched_set(c, ggml_backend_sched_new(be.data(), buft.data(), be.size(),
                    (size_t) a, b != 0, cp.op_offload));
            return 0;
        }
        case 28: ggml_backend_sched_set_copy_callback(fg107_context_access::sched(c), fg107_sched_copy_experts_cb, c); return 0;
        case 29: return fg107_compat_set_expert_pipeline(fg107_context_access::sched(c), a != 0);
        case 30: ggml_backend_sched_set_realloc_callback(fg107_context_access::sched(c),
                         [](ggml_backend_sched_t s, ggml_cgraph * g, bool x, bool y, void * u) {
                             auto * lc = static_cast<llama_context *>(u);
                             FG107ExecFrame frame(*lc);
                             fg107_goose_realloc_policy((int64_t)(intptr_t)&frame, x ? 1 : 0, y ? 1 : 0,
                                     ggml_graph_n_nodes(g), ggml_backend_sched_get_n_splits(s));
                         }, c); return 0;
        case 31: return (i64) (intptr_t) fg107_context_access::backends(c).at((size_t) a);
        case 32: return (i64) (intptr_t) fg107_context_access::backend_buft(c).at((size_t) a);
        case 33: return fg107_context_access::gf_reserve(c) && fg107_context_access::gf_reserve(c)->get_logits() != nullptr;
        case 34: ggml_opt_free(fg107_context_access::opt(c)); return 0;
        case 35: return fg107_context_access::mem(c) != nullptr;
        case 36: {
            auto & be = fg107_context_access::backends(c);
            auto & sz = fg107_context_access::buf_exp_size(c);
            if ((size_t) a >= be.size()) return 0;
            const size_t act = ggml_backend_sched_get_buffer_size(fg107_context_access::sched(c), be[(size_t) a]);
            if (sz[(size_t) a] != act) {
                LLAMA_LOG_WARN("%s: compute buffer %zu size %zu != expected %zu\n", __func__, (size_t) a, act, sz[(size_t) a]);
            }
            return 0;
        }
        case 37: return (i64) fg107_context_access::backends(c).size();
        case 38: LLAMA_LOG_INFO("%s: reserve step %lld\n", __func__, (long long) a); return 0;
        default: break;
    }
    if (op >= 50 && op <= 67) return fg107_r_state(ctx, op, a, b, d);
    GGML_ABORT("fg107_r: bad op");
}

// --- checkpoint IO primitives (dummy/host) ---------------------------------
namespace {
struct Fg107WriteDummy : llama_io_write_i {
    explicit Fg107WriteDummy(bool skip) : skip(skip) {}
    void write(const void *, size_t size) override { n += size; }
    void write_tensor(ggml_tensor *, size_t, size_t size) override { if (!skip) n += size; }
    size_t n_bytes() override { return n; }
    bool skip; size_t n = 0;
};
struct Fg107WriteHost : llama_io_write_i {
    Fg107WriteHost(uint8_t * p, size_t len) : ptr(p), cap(len) {}
    ~Fg107WriteHost() override {
        for (auto & w : wins) ggml_backend_tensor_get(w.t, w.p, w.off, w.size);
    }
    void write(const void * src, size_t size) override {
        if (size > cap) throw std::runtime_error("unexpectedly reached end of buffer");
        memcpy(ptr, src, size); ptr += size; cap -= size; n += size;
    }
    void write_tensor(ggml_tensor * t, size_t off, size_t size) override {
        wins.push_back({t, ptr, off, size}); ptr += size; cap -= size; n += size;
    }
    size_t n_bytes() override { return n; }
    struct W { ggml_tensor * t; uint8_t * p; size_t off; size_t size; };
    uint8_t * ptr; size_t cap; size_t n = 0; std::vector<W> wins;
};
struct Fg107ReadHost : llama_io_read_i {
    Fg107ReadHost(const uint8_t * p, size_t len) : ptr(p), cap(len) {}
    void read(void * dst, size_t size) override {
        if (size > cap) throw std::runtime_error("unexpectedly reached end of buffer");
        memcpy(dst, ptr, size); ptr += size; cap -= size; n += size;
    }
    void read_tensor(ggml_tensor * t, size_t off, size_t size) override {
        if (size > cap) throw std::runtime_error("unexpectedly reached end of buffer");
        ggml_backend_tensor_set(t, ptr, off, size); ptr += size; cap -= size; n += size;
    }
    void discard() override {}
    size_t n_bytes() override { return n; }
    const uint8_t * ptr; size_t cap; size_t n = 0;
};
} // namespace

extern "C" i64 fg107_r(i64 ctx, i64 op, i64 a, i64 b, i64 d); // fwd (defined above)

extern "C" i64 fg107_r_state(i64 ctx, i64 op, i64 a, i64 b, i64 d) {    auto & F = fr(ctx);
    auto * c = F.lctx;
    try {
        switch (op) {
            case 50: F.wio.reset(fgcio_adapter_write_dummy(a != 0)); if (!F.wio) throw std::bad_alloc(); return 0;
            case 51: F.wio.reset(fgcio_adapter_write_host((uint8_t *) (intptr_t) a, (size_t) b)); if (!F.wio) throw std::bad_alloc(); return 0;
            case 52: F.rio.reset(fgcio_adapter_read_host((const uint8_t *) (intptr_t) a, (size_t) b)); if (!F.rio) throw std::bad_alloc(); return 0;
            case 53: { uint32_t v = (uint32_t) a; F.wio->write(&v, sizeof v); return 0; }
            case 54: { uint32_t v = 0; F.rio->read(&v, sizeof v); return v; }
            case 55: { llama_seq_id s = (llama_seq_id) a; F.wio->write(&s, sizeof s); return 0; }
            case 56: { llama_seq_id s = 0; F.rio->read(&s, sizeof s); return s; }
            case 57: return (i64) fg107_context_access::state_write(c, *F.wio);
            case 58: return (i64) fg107_context_access::state_read(c, *F.rio);
            case 59: return (i64) fg107_context_access::seq_write(c, *F.wio, (llama_seq_id) a, (llama_state_seq_flags) b);
            case 60: return (i64) fg107_context_access::seq_read(c, *F.rio, (llama_seq_id) a, (llama_state_seq_flags) b);
            case 61: return (i64) F.wio->n_bytes();
            case 62: F.rio->discard(); return 0;
            case 63: return fgcio_context_has((i64)(intptr_t)c, a);
            case 64: {
                const auto handle = fgcio_context_write_device((i64)(intptr_t)c, d, a, b);
                F.wio.reset(fgcio_adapter_write_device(handle, (uint8_t *)(intptr_t)a, (size_t)b));
                if (!F.wio) throw std::bad_alloc();
                return 0;
            }
            case 65: {
                const auto handle = fgcio_context_read_device((i64)(intptr_t)c, d, a, b);
                F.rio.reset(fgcio_adapter_read_device_take(handle));
                if (!F.rio) throw std::bad_alloc();
                return 0;
            }
            case 66: return (i64) LLAMA_STATE_SEQ_FLAGS_ON_DEVICE;
            case 67: return ((llama_state_seq_flags) a & LLAMA_STATE_SEQ_FLAGS_ON_DEVICE) != 0;
            default: GGML_ABORT("fg107_r_state: bad op");
        }
    } catch (const std::exception & e) {
        LLAMA_LOG_ERROR("%s: state op %lld failed: %s\n", __func__, (long long) op, e.what());
        return -1;
    }
}

// ---- policy/control services (context-policy.goose) -----------------------
extern "C" i64 fg107_pol(i64 ctx, i64 op, i64 a, i64 b, i64 c, i64 d) {
    auto & F = fr(ctx);
    auto * lc = F.lctx;
    auto & cp = fg107_context_access::cp(lc);
    auto & nodes = fg107_context_access::gf_reserve(lc)->get_fused_nodes();
    switch (op) {
        case 0: switch (a) {
            case 0: return cp.flash_attn; case 1: return cp.fused_gdn_ar; case 2: return cp.fused_gdn_ch;
            case 3: return cp.fused_lid; case 4: return cp.fused_dsv4_hc_pre;
            case 5: return cp.fused_dsv4_hc_comb; case 6: return cp.fused_dsv4_hc_post;
            default: GGML_ABORT("fg107_pol: bad fused field"); }
        case 1: switch (a) {
            case 0: cp.flash_attn = b != 0; return 0; case 1: cp.fused_gdn_ar = b != 0; return 0;
            case 2: cp.fused_gdn_ch = b != 0; return 0; case 3: cp.fused_lid = b != 0; return 0;
            case 4: cp.fused_dsv4_hc_pre = b != 0; return 0; case 5: cp.fused_dsv4_hc_comb = b != 0; return 0;
            case 6: cp.fused_dsv4_hc_post = b != 0; return 0; default: GGML_ABORT("fg107_pol: bad fused field"); }
        case 2: switch (a) {
            case 0: return cp.auto_fa; case 1: return cp.auto_fgdn; case 2: return cp.auto_flid;
            case 3: return cp.auto_fhc; default: GGML_ABORT("fg107_pol: bad auto field"); }
        case 3: switch (a) {
            case 0: cp.auto_fa = false; return 0; case 1: cp.auto_fgdn = false; return 0;
            case 2: cp.auto_flid = false; return 0; case 3: cp.auto_fhc = false; return 0;
            default: GGML_ABORT("fg107_pol: bad auto field"); }
        case 4: return (i64) (intptr_t) lc->graph_reserve((uint32_t) a, (uint32_t) b, (uint32_t) a,
                    (const llama_memory_context_i *) (intptr_t) c, true);
        case 5: return (i64) nodes.size();
        case 6: return (i64) nodes[(size_t) a].op;
        case 7: return (i64) nodes[(size_t) a].il;
        case 8: return (i64) (intptr_t) nodes[(size_t) a].tensor;
        case 9: return (i64) (intptr_t) ggml_backend_sched_get_tensor_backend(
                    fg107_context_access::sched(lc), (ggml_tensor *) (intptr_t) a);
        case 10: {
            auto * be = (ggml_backend_t) (intptr_t) a;
            return be ? (i64) (intptr_t) ggml_backend_get_device(be) : 0;
        }
        case 11: return (i64) (intptr_t) fg107_context_access::dev_layer(lc, (int) a);
        case 12: switch (a) {
            case 0: LLAMA_LOG_WARN("%s: failed to reserve graph for fused-op probe\n", __func__); return 0;
            case 1: LLAMA_LOG_WARN("%s: fused node layer %lld device mismatch\n", __func__, (long long) b); return 0;
            case 2: LLAMA_LOG_INFO("%s: fused op %lld enabled\n", __func__, (long long) b); return 0;
            case 3: LLAMA_LOG_WARN("%s: fused op %lld not supported, disabled\n", __func__, (long long) b); return 0;
            case 4: LLAMA_LOG_INFO("%s: resolving fused Gated Delta Net support\n", __func__); return 0;
            case 5: LLAMA_LOG_INFO("%s: resolving fused Lightning Indexer support\n", __func__); return 0;
            case 6: LLAMA_LOG_INFO("%s: resolving fused DeepSeek V4 HC support\n", __func__); return 0;
            case 7: LLAMA_LOG_INFO("%s: graph reallocation: nodes=%lld splits=%lld same=%lld diff=%lld\n",
                        __func__, (long long) b, (long long) c, (long long) (d / 1000000), (long long) (d % 1000000)); return 0;
            default: GGML_ABORT("fg107_pol: bad log site"); }
        case 13: switch (a) {
            case 0: return LLM_FUSED_OP_FLASH_ATTN; case 1: return LLM_FUSED_OP_GDN_AR;
            case 2: return LLM_FUSED_OP_GDN_CH; case 3: return LLM_FUSED_OP_LIGHTNING_INDEXER;
            case 4: return LLM_FUSED_OP_DSV4_HC_PRE; case 5: return LLM_FUSED_OP_DSV4_HC_COMB;
            case 6: return LLM_FUSED_OP_DSV4_HC_POST; default: GGML_ABORT("fg107_pol: bad probe"); }
        case 14: switch (a) {
            case 2: return 16; default: return 1; }
        case 15: if (a == 0) fg107_context_access::reallocs_diff(lc)++; else fg107_context_access::reallocs_same(lc)++; return 0;
        case 16: return a == 0 ? fg107_context_access::reallocs_same(lc) : fg107_context_access::reallocs_diff(lc);
        default: GGML_ABORT("fg107_pol: bad op");
    }
}

extern "C" i64 fg107_cpi(i64 ctx, i64 op, i64 field, i64 value) {
    auto & F = fr(ctx);
    auto * lc = F.lctx;
    auto & cp = fg107_context_access::cp(lc);
    auto & hp = fg107_context_access::model(lc).hparams;
    const auto * p = (const llama_context_params *) F.params;
    if (!p) return 0;
    if (op == 0) {
        switch (field) {
            case 0: return p->rope_scaling_type; case 1: return p->attention_type;
            case 2: return p->flash_attn_type; case 3: return p->n_batch; case 4: return p->n_ubatch;
            case 5: return p->n_outputs_max; case 6: return p->n_outputs_max_per_seq;
            case 200: return hp.rope_scaling_type_train; case 201: return hp.pooling_type;
            case 202: return hp.causal_attn; case 203: return (i64) fg107_context_access::model(lc).arch;
            case 400: return cp.rope_scaling_type; case 401: return cp.pooling_type;
            case 402: return cp.causal_attn; case 403: return cp.flash_attn; case 404: return cp.auto_fa;
            case 405: return cp.fused_gdn_ar; case 406: return cp.fused_gdn_ch; case 407: return cp.auto_fgdn;
            case 408: return cp.fused_lid; case 409: return cp.auto_flid; case 410: return cp.fused_dsv4_hc_pre;
            case 411: return cp.fused_dsv4_hc_comb; case 412: return cp.fused_dsv4_hc_post; case 413: return cp.auto_fhc;
            case 414: return cp.n_batch; case 415: return cp.n_ubatch; case 416: return cp.n_outputs_max;
            case 417: return cp.n_outputs_max_per_seq; case 418: return cp.n_ctx;
            default: GGML_ABORT("fg107_cpi: bad get field");
        }
    }
    switch (field) {
        case 400: cp.rope_scaling_type = (decltype(cp.rope_scaling_type)) value; return 0;
        case 401: cp.pooling_type = (decltype(cp.pooling_type)) value; return 0;
        case 402: cp.causal_attn = value != 0; return 0;
        case 403: cp.flash_attn = value != 0; return 0; case 404: cp.auto_fa = value != 0; return 0;
        case 405: cp.fused_gdn_ar = value != 0; return 0; case 406: cp.fused_gdn_ch = value != 0; return 0;
        case 407: cp.auto_fgdn = value != 0; return 0; case 408: cp.fused_lid = value != 0; return 0;
        case 409: cp.auto_flid = value != 0; return 0; case 410: cp.fused_dsv4_hc_pre = value != 0; return 0;
        case 411: cp.fused_dsv4_hc_comb = value != 0; return 0; case 412: cp.fused_dsv4_hc_post = value != 0; return 0;
        case 413: cp.auto_fhc = value != 0; return 0; case 414: cp.n_batch = (uint32_t) value; return 0;
        case 415: cp.n_ubatch = (uint32_t) value; return 0; case 416: cp.n_outputs_max = (uint32_t) value; return 0;
        case 417: cp.n_outputs_max_per_seq = (uint32_t) value; return 0;
        default: GGML_ABORT("fg107_cpi: bad set field");
    }
}

extern "C" float fg107_cpf(i64 ctx, i64 op, i64 field, float value) {
    auto & F = fr(ctx);
    auto * lc = F.lctx;
    auto & cp = fg107_context_access::cp(lc);
    auto & hp = fg107_context_access::model(lc).hparams;
    const auto * p = (const llama_context_params *) F.params;
    if (!p) return 0.0f;
    if (op == 0) {
        switch (field) {
            case 100: return p->rope_freq_base; case 101: return p->rope_freq_scale;
            case 102: return p->yarn_ext_factor; case 103: return p->yarn_attn_factor;
            case 300: return hp.rope_freq_base_train; case 301: return hp.rope_freq_scale_train;
            case 302: return hp.rope_yarn_log_mul; case 303: return hp.rope_attn_factor;
            case 500: return cp.rope_freq_scale; case 501: return cp.yarn_ext_factor;
            case 502: return cp.yarn_attn_factor;
            default: GGML_ABORT("fg107_cpf: bad get field");
        }
    }
    switch (field) {
        case 500: cp.rope_freq_scale = value; return 0; case 501: cp.yarn_ext_factor = value; return 0;
        case 502: cp.yarn_attn_factor = value; return 0;
        default: GGML_ABORT("fg107_cpf: bad set field");
    }
}

extern "C" i64 fg107_cpc(i64 ctx, i64 which) {
    (void) ctx;
    switch (which) {
        case 0: return LLAMA_ROPE_SCALING_TYPE_UNSPECIFIED; case 1: return LLAMA_ROPE_SCALING_TYPE_NONE;
        case 2: return LLAMA_ROPE_SCALING_TYPE_YARN; case 3: return LLAMA_POOLING_TYPE_UNSPECIFIED;
        case 4: return LLAMA_POOLING_TYPE_NONE; case 5: return LLAMA_ATTENTION_TYPE_UNSPECIFIED;
        case 6: return LLAMA_ATTENTION_TYPE_CAUSAL; case 7: return LLAMA_FLASH_ATTN_TYPE_DISABLED;
        case 8: return LLAMA_FLASH_ATTN_TYPE_AUTO; case 9: return LLM_ARCH_DEEPSEEK2;
        default: GGML_ABORT("fg107_cpc: bad constant");
    }
}

extern "C" i64 fg107_cplog(i64 ctx, i64 site, i64 a, i64 b) {
    (void) ctx; (void) a; (void) b;
    switch (site) {
        case 0: LLAMA_LOG_WARN("%s: setting new yarn_attn_factor from yarn_log_mul\n", __func__); return 0;
        default: GGML_ABORT("fg107_cplog: bad site");
    }
}

extern "C" i64 fg107_cpmodel(i64 ctx) {
    auto * lc = fr(ctx).lctx;
    return llama_model_has_encoder(&fg107_context_access::model(lc)) ? 1 : 0;
}

