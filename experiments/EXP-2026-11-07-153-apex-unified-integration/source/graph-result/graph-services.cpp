// worker-7: typed primitives for the Apex graph-result/input block.
//
// Every loop and decision lives in graph-result.goose / graph-input.goose /
// graph-context.goose. This TU only converts opaque handles to the real
// llama/ggml types and performs storage/backend effects.
#include "graph-services.h"

#include "llama-graph.h"
#include "llama-impl.h"
#include "llama-batch.h"
#include "llama-hparams.h"
#include "llama-cparams.h"

#include "ggml.h"
#include "ggml-backend.h"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

static int64_t H(const void * p) { return (int64_t) (intptr_t) p; }
static ggml_tensor * T(int64_t h) { return (ggml_tensor *) (intptr_t) h; }
static llm_graph_result * R(int64_t h) { return (llm_graph_result *) (intptr_t) h; }
static const llm_graph_params * P(int64_t h) { return (const llm_graph_params *) (intptr_t) h; }
static const llama_ubatch * U(int64_t h) { return (const llama_ubatch *) (intptr_t) h; }

static std::vector<ggml_tensor *> & sampling(llm_graph_result * r, int64_t kind) {
    switch (kind) {
        case 0: return r->t_sampled;
        case 1: return r->t_sampled_probs;
        case 2: return r->t_sampled_logits;
        case 3: return r->t_candidates;
        default: GGML_ABORT("fg153: bad sampling kind");
    }
}

extern "C" int64_t fg153_gr_const(int64_t code) {
    switch (code) {
        case 0: return LLAMA_MAX_LAYERS;
        default: GGML_ABORT("fg153_gr_const: bad code");
    }
}

// ---------------------------------------------------------------------------
// Generic tensor access.
// ---------------------------------------------------------------------------
extern "C" int64_t fg153_gx_ne(int64_t t, int64_t i) {
    GGML_ASSERT(t != 0 && i >= 0 && i < 4);
    return T(t)->ne[i];
}
extern "C" int64_t fg153_gx_nb(int64_t t, int64_t i) {
    GGML_ASSERT(t != 0 && i >= 0 && i < 4);
    return (int64_t) T(t)->nb[i];
}
extern "C" int64_t fg153_gx_elem(int64_t t) {
    GGML_ASSERT(t != 0);
    return (int64_t) ggml_element_size(T(t));
}
extern "C" int64_t fg153_gx_nbytes(int64_t t) {
    GGML_ASSERT(t != 0);
    return (int64_t) ggml_nbytes(T(t));
}
extern "C" int64_t fg153_gx_is_host(int64_t t) {
    GGML_ASSERT(t != 0);
    return T(t)->buffer && ggml_backend_buffer_is_host(T(t)->buffer);
}
extern "C" void fg153_gx_set_output(int64_t t) {
    if (t != 0) {
        ggml_set_output(T(t));
    }
}
extern "C" void fg153_gx_store_i32(int64_t t, int64_t idx, int64_t v) {
    GGML_ASSERT(t != 0 && idx >= 0 && idx < ggml_nelements(T(t)));
    GGML_ASSERT(ggml_backend_buffer_is_host(T(t)->buffer));
    ((int32_t *) T(t)->data)[idx] = (int32_t) v;
}
extern "C" void fg153_gx_store_i64(int64_t t, int64_t idx, int64_t v) {
    GGML_ASSERT(t != 0 && idx >= 0 && idx < ggml_nelements(T(t)));
    GGML_ASSERT(ggml_backend_buffer_is_host(T(t)->buffer));
    ((int64_t *) T(t)->data)[idx] = v;
}
extern "C" void fg153_gx_store_f32(int64_t t, int64_t idx, float v) {
    GGML_ASSERT(t != 0 && idx >= 0 && idx < ggml_nelements(T(t)));
    GGML_ASSERT(ggml_backend_buffer_is_host(T(t)->buffer));
    ((float *) T(t)->data)[idx] = v;
}
extern "C" void fg153_gx_store_f64(int64_t t, int64_t idx, double v) {
    GGML_ASSERT(t != 0 && idx >= 0 && idx < ggml_nelements(T(t)));
    GGML_ASSERT(ggml_backend_buffer_is_host(T(t)->buffer));
    ((double *) T(t)->data)[idx] = v;
}
extern "C" void fg153_gx_set_i32(int64_t t, int64_t idx, int64_t v) {
    GGML_ASSERT(t != 0 && idx >= 0 && idx < ggml_nelements(T(t)));
    const int32_t x = (int32_t) v;
    ggml_backend_tensor_set(T(t), &x, (size_t) idx * sizeof(x), sizeof(x));
}
extern "C" void fg153_gx_set_i64(int64_t t, int64_t idx, int64_t v) {
    GGML_ASSERT(t != 0 && idx >= 0 && idx < ggml_nelements(T(t)));
    const int64_t x = v;
    ggml_backend_tensor_set(T(t), &x, (size_t) idx * sizeof(x), sizeof(x));
}
extern "C" void fg153_gx_set_f32(int64_t t, int64_t idx, float v) {
    GGML_ASSERT(t != 0 && idx >= 0 && idx < ggml_nelements(T(t)));
    ggml_backend_tensor_set(T(t), &v, (size_t) idx * sizeof(v), sizeof(v));
}
extern "C" void fg153_gx_set_bytes(int64_t t, int64_t src, int64_t off, int64_t size) {
    GGML_ASSERT(t != 0 && src != 0 && off >= 0 && size >= 0);
    GGML_ASSERT((uint64_t) off + (uint64_t) size <= ggml_nbytes(T(t)));
    ggml_backend_tensor_set(T(t), (const void *) (intptr_t) src, (size_t) off, (size_t) size);
}
extern "C" void fg153_gx_assert(int64_t ok) {
    if (!ok) {
        GGML_ABORT("fg153: Goose invariant failed");
    }
}
extern "C" void fg153_gx_fail(void) {
    GGML_ABORT("fg153: Goose failure");
}
extern "C" void fg153_gx_log(int64_t site, int64_t a, int64_t b) {
    switch (site) {
        case 0: LLAMA_LOG_DEBUG("%s: cannot reuse graph due to incompatible graph parameters\n", "llm_graph_result::can_reuse"); break;
        case 1: LLAMA_LOG_DEBUG("%s: checking compatibility of %d inputs:\n", "llm_graph_result::can_reuse", (int) a); break;
        case 2: LLAMA_LOG_DEBUG("%s: can_reuse = %d\n", "input", (int) a); break;
        case 3: LLAMA_LOG_DEBUG("%s: can reuse graph = %d\n", "llm_graph_result::can_reuse", (int) a); break;
        case 4: LLAMA_LOG_DEBUG("%s: n_tokens = %lld, n_outputs = %lld\n", "graph", (long long) a, (long long) b); break;
        default: GGML_ABORT("fg153_gx_log: bad site");
    }
}

// ---------------------------------------------------------------------------
// llm_graph_result.
// ---------------------------------------------------------------------------
extern "C" int64_t fg153_gr_tensor_get(int64_t r, int64_t slot) {
    llm_graph_result * x = R(r);
    switch (slot) {
        case 0: return H(x->t_inp_tokens);
        case 1: return H(x->t_inp_embd);
        case 2: return H(x->t_logits);
        case 3: return H(x->t_embd);
        case 4: return H(x->t_embd_pooled);
        case 5: return H(x->t_h_nextn);
        default: GGML_ABORT("fg153_gr_tensor_get: bad slot");
    }
}
extern "C" void fg153_gr_tensor_set(int64_t r, int64_t slot, int64_t t) {
    llm_graph_result * x = R(r);
    switch (slot) {
        case 0: x->t_inp_tokens  = T(t); break;
        case 1: x->t_inp_embd    = T(t); break;
        case 2: x->t_logits      = T(t); break;
        case 3: x->t_embd        = T(t); break;
        case 4: x->t_embd_pooled = T(t); break;
        case 5: x->t_h_nextn     = T(t); break;
        default: GGML_ABORT("fg153_gr_tensor_set: bad slot");
    }
}
extern "C" int64_t fg153_gr_layer_inp_len(int64_t r) {
    return (int64_t) R(r)->t_layer_inp.size();
}
extern "C" void fg153_gr_layer_inp_reset(int64_t r, int64_t n) {
    GGML_ASSERT(n >= 0);
    auto & v = R(r)->t_layer_inp;
    v.assign((size_t) n, nullptr);
}
extern "C" int64_t fg153_gr_layer_inp_get(int64_t r, int64_t i) {
    auto & v = R(r)->t_layer_inp;
    GGML_ASSERT(i >= 0 && (size_t) i < v.size());
    return H(v[(size_t) i]);
}
extern "C" void fg153_gr_layer_inp_set(int64_t r, int64_t i, int64_t t) {
    auto & v = R(r)->t_layer_inp;
    GGML_ASSERT(i >= 0 && (size_t) i < v.size());
    v[(size_t) i] = T(t);
}
extern "C" int64_t fg153_gr_sampling_len(int64_t r, int64_t kind) {
    return (int64_t) sampling(R(r), kind).size();
}
extern "C" int64_t fg153_gr_sampling_get(int64_t r, int64_t kind, int64_t i) {
    auto & v = sampling(R(r), kind);
    GGML_ASSERT(i >= 0 && (size_t) i < v.size());
    return H(v[(size_t) i]);
}
extern "C" void fg153_gr_sampling_clear(int64_t r, int64_t kind) {
    sampling(R(r), kind).clear();
}
extern "C" int64_t fg153_gr_inputs_len(int64_t r) {
    return (int64_t) R(r)->inputs.size();
}
extern "C" int64_t fg153_gr_input_at(int64_t r, int64_t i) {
    auto & v = R(r)->inputs;
    GGML_ASSERT(i >= 0 && (size_t) i < v.size());
    return H(v[(size_t) i].get());
}
extern "C" int64_t fg153_gr_inputs_add(int64_t r, int64_t raw_input) {
    GGML_ASSERT(raw_input != 0);
    llm_graph_input_ptr p((llm_graph_input_i *) (intptr_t) raw_input);
    auto & v = R(r)->inputs;
    v.emplace_back(std::move(p));
    return H(v.back().get());
}
extern "C" void fg153_gr_inputs_clear(int64_t r) {
    R(r)->inputs.clear();
}
extern "C" int64_t fg153_gi_set_input(int64_t inp, int64_t ubatch) {
    GGML_ASSERT(inp != 0 && ubatch != 0);
    ((llm_graph_input_i *) (intptr_t) inp)->set_input(U(ubatch));
    return 0;
}
extern "C" int64_t fg153_gi_can_reuse(int64_t inp, int64_t params) {
    GGML_ASSERT(inp != 0 && params != 0);
    return ((llm_graph_input_i *) (intptr_t) inp)->can_reuse(*P(params)) ? 1 : 0;
}
extern "C" int64_t fg153_gr_fused_len(int64_t r) {
    return (int64_t) R(r)->fused_nodes.size();
}
extern "C" void fg153_gr_fused_push(int64_t r, int64_t op, int64_t tensor, int64_t il) {
    R(r)->fused_nodes.push_back({ (llm_fused_op) op, T(tensor), (int) il });
}
extern "C" int64_t fg153_gr_fused_op_at(int64_t r, int64_t i) {
    auto & v = R(r)->fused_nodes;
    GGML_ASSERT(i >= 0 && (size_t) i < v.size());
    return (int64_t) v[(size_t) i].op;
}
extern "C" int64_t fg153_gr_fused_tensor_at(int64_t r, int64_t i) {
    auto & v = R(r)->fused_nodes;
    GGML_ASSERT(i >= 0 && (size_t) i < v.size());
    return H(v[(size_t) i].tensor);
}
extern "C" int64_t fg153_gr_fused_il_at(int64_t r, int64_t i) {
    auto & v = R(r)->fused_nodes;
    GGML_ASSERT(i >= 0 && (size_t) i < v.size());
    return v[(size_t) i].il;
}
extern "C" void fg153_gr_fused_clear(int64_t r) {
    R(r)->fused_nodes.clear();
}
extern "C" int64_t fg153_gr_hdmd_get(int64_t r, int64_t cur, int64_t rot) {
    auto & m = R(r)->hdmd_inputs;
    const auto it = m.find({ (const ggml_tensor *) (intptr_t) cur, (const ggml_tensor *) (intptr_t) rot });
    return it == m.end() ? 0 : H(it->second);
}
extern "C" void fg153_gr_hdmd_set(int64_t r, int64_t cur, int64_t rot, int64_t res) {
    R(r)->hdmd_inputs[{(const ggml_tensor *) (intptr_t) cur, (const ggml_tensor *) (intptr_t) rot}] = T(res);
}
extern "C" void fg153_gr_hdmd_clear(int64_t r) {
    R(r)->hdmd_inputs.clear();
}
extern "C" void fg153_gr_params_clear(int64_t params_ptr) {
    GGML_ASSERT(params_ptr != 0);
    *(llm_graph_params *) (intptr_t) params_ptr = llm_graph_params {};
}
extern "C" void fg153_gr_params_copy(int64_t dst_params, int64_t src_params) {
    GGML_ASSERT(dst_params != 0 && src_params != 0);
    *(llm_graph_params *) (intptr_t) dst_params = *P(src_params);
}
extern "C" int64_t fg153_gr_get_max_nodes(int64_t r) {
    return R(r)->get_max_nodes();
}
extern "C" int64_t fg153_gr_meta_size(int64_t r) {
    return (int64_t) R(r)->buf_compute_meta.size();
}
extern "C" void fg153_gr_meta_resize(int64_t r, int64_t bytes) {
    GGML_ASSERT(bytes >= 0);
    R(r)->buf_compute_meta.resize((size_t) bytes);
}
extern "C" int64_t fg153_gr_ctx_init(int64_t r) {
    llm_graph_result * x = R(r);
    ggml_init_params ip = {
        /*.mem_size   =*/ x->buf_compute_meta.size(),
        /*.mem_buffer =*/ x->buf_compute_meta.data(),
        /*.no_alloc   =*/ true,
    };
    x->ctx_compute.reset(ggml_init(ip));
    return H(x->ctx_compute.get());
}
extern "C" void fg153_gr_ctx_set(int64_t r, int64_t ctx) {
    R(r)->ctx_compute.reset((ggml_context *) (intptr_t) ctx);
}
extern "C" int64_t fg153_gr_ctx_get(int64_t r) {
    return H(R(r)->ctx_compute.get());
}
extern "C" int64_t fg153_gr_new_graph(int64_t ctx, int64_t max_nodes) {
    GGML_ASSERT(ctx != 0 && max_nodes > 0);
    return H(ggml_new_graph_custom((ggml_context *) (intptr_t) ctx, (size_t) max_nodes, false));
}
extern "C" void fg153_gr_gf_set(int64_t r, int64_t gf) {
    R(r)->gf = (ggml_cgraph *) (intptr_t) gf;
}
extern "C" int64_t fg153_gr_gf_get(int64_t r) {
    return H(R(r)->gf);
}
extern "C" int64_t fg153_gr_tensor_overhead(void) {
    return (int64_t) ggml_tensor_overhead();
}
extern "C" int64_t fg153_gr_graph_overhead(int64_t max_nodes) {
    return (int64_t) ggml_graph_overhead_custom((size_t) max_nodes, false);
}

// ---------------------------------------------------------------------------
// llm_graph_params.
// ---------------------------------------------------------------------------
extern "C" int64_t fg153_gp_get(int64_t params, int64_t k, int64_t) {
    const llm_graph_params * p = P(params);
    switch (k) {
        case 0:  return (int64_t) p->arch;
        case 1:  return p->n_outputs;
        case 2:  return (int64_t) p->gtype;
        case 3:  return p->cvec != nullptr;
        case 4:  return p->loras != nullptr;
        case 5:  return p->cross != nullptr;
        case 6:  return p->mctx != nullptr;
        case 7:  return (int64_t) p->samplers.size();
        case 8:  return p->cparams.embeddings;
        case 9:  return p->cparams.embeddings_nextn;
        case 10: return p->cparams.embeddings_nextn_masked;
        case 11: return p->cparams.causal_attn;
        case 12: return p->cparams.nextn_layer_offset;
        case 13: return (int64_t) p->cparams.pooling_type;
        case 14: return p->cparams.kv_unified;
        case 15: return p->cparams.flash_attn;
        case 16: return p->cparams.training;
        case 17: return p->cparams.n_ctx;
        case 18: return p->cparams.n_outputs_max_per_seq;
        default: GGML_ABORT("fg153_gp_get: bad field");
    }
}
extern "C" int64_t fg153_gp_sampler_key(int64_t params, int64_t i) {
    auto & m = P(params)->samplers;
    GGML_ASSERT(i >= 0 && (size_t) i < m.size());
    auto it = m.begin();
    std::advance(it, (long) i);
    return (int64_t) it->first;
}
extern "C" int64_t fg153_gp_sampler_val(int64_t params, int64_t i) {
    auto & m = P(params)->samplers;
    GGML_ASSERT(i >= 0 && (size_t) i < m.size());
    auto it = m.begin();
    std::advance(it, (long) i);
    return H(it->second);
}
extern "C" int64_t fg153_gp_layer_inp_size(int64_t params) {
    return (int64_t) P(params)->cparams.embeddings_layer_inp.size();
}
extern "C" int64_t fg153_gp_layer_inp_at(int64_t params, int64_t i) {
    auto & v = P(params)->cparams.embeddings_layer_inp;
    GGML_ASSERT(i >= 0 && (size_t) i < v.size());
    return v[(size_t) i] ? 1 : 0;
}
extern "C" int64_t fg153_gp_ubatch(int64_t params) {
    return H(&P(params)->ubatch);
}

// ---------------------------------------------------------------------------
// llama_ubatch.
// ---------------------------------------------------------------------------
extern "C" int64_t fg153_gu_get(int64_t u, int64_t k, int64_t i) {
    const llama_ubatch * x = U(u);
    switch (k) {
        case 0:  return x->n_tokens;
        case 1:  return x->n_seq_tokens;
        case 2:  return x->n_seqs;
        case 3:  return x->n_seqs_unq;
        case 4:  return x->n_pos;
        case 5:  return x->equal_seqs();
        case 6:  return x->is_mixed();
        case 7:  return x->token != nullptr;
        case 8:  return x->embd != nullptr;
        case 9:  return x->embd_state != nullptr;
        case 10: return x->pos != nullptr;
        case 11: return x->output != nullptr;
        case 12: return x->data != nullptr;
        case 13: GGML_ASSERT(i >= 0 && (uint64_t) i < x->n_tokens); return x->token[i];
        case 14: GGML_ASSERT(x->type && i >= 0 && (uint64_t) i < x->n_tokens); return x->type[i];
        case 15: GGML_ASSERT(x->pos && i >= 0 && (uint64_t) i < (uint64_t) x->n_tokens * x->n_pos); return x->pos[i];
        case 16: GGML_ASSERT(x->output && i >= 0 && (uint64_t) i < x->n_tokens); return x->output[i];
        case 17: GGML_ASSERT(x->n_seq_id && i >= 0 && (uint64_t) i < x->n_tokens); return x->n_seq_id[i];
        case 18: GGML_ASSERT(x->seq_id_unq && i >= 0 && (uint64_t) i < x->n_seqs_unq); return x->seq_id_unq[i];
        case 19: return H(x->token);
        case 20: return H(x->embd);
        case 21: return H(x->embd_state);
        case 22: return H(x->pos);
        case 23: return H(x->output);
        case 24: return H(x->type);
        default: GGML_ABORT("fg153_gu_get: bad field");
    }
}
extern "C" int64_t fg153_gu_seq_id(int64_t u, int64_t i, int64_t s) {
    const llama_ubatch * x = U(u);
    GGML_ASSERT(x->seq_id && i >= 0 && (uint64_t) i < x->n_tokens);
    GGML_ASSERT(x->n_seq_id && s >= 0 && s < x->n_seq_id[i]);
    return x->seq_id[i][s];
}

// ---------------------------------------------------------------------------
// Graph inputs.
// ---------------------------------------------------------------------------
static llm_graph_input_embd   * IE0(int64_t p) { return (llm_graph_input_embd *) (intptr_t) p; }
static llm_graph_input_embd_h * IE1(int64_t p) { return (llm_graph_input_embd_h *) (intptr_t) p; }
static llm_graph_input_pos    * IE2(int64_t p) { return (llm_graph_input_pos *) (intptr_t) p; }
static llm_graph_input_out_ids * IE3(int64_t p) { return (llm_graph_input_out_ids *) (intptr_t) p; }

extern "C" int64_t fg153_gi_get(int64_t inp, int64_t kind, int64_t k, int64_t) {
    switch (kind) {
        case 0: {
            auto * x = IE0(inp);
            switch (k) {
                case 0: return H(x->tokens);
                case 1: return H(x->embd);
                case 2: return H(x->mixed_tokens);
                case 3: return H(x->mixed_slots);
                case 4: return H(x->mixed_embd);
                case 5: return H(x->scale_rows);
                case 6: return x->n_embd;
                default: GGML_ABORT("fg153_gi_get: bad embd field");
            }
        }
        case 1: {
            auto * x = IE1(inp);
            switch (k) {
                case 0: return H(x->tokens);
                case 1: return H(x->embd);
                case 2: return H(x->h);
                case 3: return x->n_embd;
                case 4: return x->n_embd_state;
                default: GGML_ABORT("fg153_gi_get: bad embd_h field");
            }
        }
        case 2: {
            auto * x = IE2(inp);
            switch (k) {
                case 0: return H(x->pos);
                case 1: return x->n_pos_per_embd;
                case 2: return (int64_t) x->rope_section_order;
                default: GGML_ABORT("fg153_gi_get: bad pos field");
            }
        }
        case 3: {
            auto * x = IE3(inp);
            switch (k) {
                case 0: return H(x->out_ids);
                case 1: return x->n_outputs;
                default: GGML_ABORT("fg153_gi_get: bad out_ids field");
            }
        }
        default: GGML_ABORT("fg153_gi_get: bad kind");
    }
}
extern "C" float fg153_gi_fget(int64_t inp, int64_t kind, int64_t k) {
    GGML_ASSERT(kind == 0 && k == 7);
    return IE0(inp)->scale_tok;
}
extern "C" void fg153_gi_set(int64_t inp, int64_t kind, int64_t k, int64_t v) {
    switch (kind) {
        case 0: {
            auto * x = IE0(inp);
            switch (k) {
                case 0: x->tokens = T(v); break;
                case 1: x->embd = T(v); break;
                case 2: x->mixed_tokens = T(v); break;
                case 3: x->mixed_slots = T(v); break;
                case 4: x->mixed_embd = T(v); break;
                case 5: x->scale_rows = T(v); break;
                default: GGML_ABORT("fg153_gi_set: bad embd field");
            }
            return;
        }
        case 1: {
            auto * x = IE1(inp);
            switch (k) {
                case 0: x->tokens = T(v); break;
                case 1: x->embd = T(v); break;
                case 2: x->h = T(v); break;
                default: GGML_ABORT("fg153_gi_set: bad embd_h field");
            }
            return;
        }
        case 2: IE2(inp)->pos = T(v); return;
        case 3: IE3(inp)->out_ids = T(v); return;
        default: GGML_ABORT("fg153_gi_set: bad kind");
    }
}

// ---------------------------------------------------------------------------
// llm_graph_context metadata.
// ---------------------------------------------------------------------------
extern "C" int64_t fg153_gh_get(int64_t params, int64_t k, int64_t i) {
    const llm_graph_params * p = P(params);
    const llama_hparams & h = p->hparams;
    const llama_cparams & c = p->cparams;
    switch (k) {
        case 0:  return (int64_t) p->arch;
        case 1:  return h.n_embd;
        case 2:  return h.n_layer();
        case 3:  return h.n_layer_nextn;
        case 4:  return h.n_rot();
        case 5:  return h.n_head();
        case 6:  return h.n_head_kv();
        case 7:  return h.n_embd_head_k();
        case 8:  return h.n_embd_k_gqa();
        case 9:  return h.n_embd_head_v();
        case 10: return h.n_embd_v_gqa();
        case 11: return h.n_expert;
        case 12: return h.n_expert_used();
        case 15: return (int64_t) h.rope_type;
        case 16: return c.n_ctx;
        case 23: return c.n_ctx_orig_yarn;
        case 24: return (int64_t) c.pooling_type;
        case 25: return c.warmup;
        case 26: return p->ubatch.n_tokens;
        case 27: return p->n_outputs;
        case 28: return H(p->sched);
        case 29: return H(p->backend_cpu);
        case 30: return H(p->cvec);
        case 31: return H(p->loras);
        case 32: return H(p->mctx);
        case 33: return H(p->cross);
        case 34: return H(p->moe_cache);
        case 35: return H(p->prec_policy);
        case 36: return H(p->hdmd);
        case 37: return H(p->res);
        default: GGML_ABORT("fg153_gh_get: bad field");
    }
}

// Float metadata reads keep IEEE bit patterns intact; graph-context.goose uses
// f32 arguments for the float slots.
extern "C" float fg153_gh_fget(int64_t params, int64_t k) {
    const llm_graph_params * p = P(params);
    const llama_hparams & h = p->hparams;
    const llama_cparams & c = p->cparams;
    switch (k) {
        case 0: return c.rope_freq_base;
        case 1: return c.rope_freq_scale;
        case 2: return c.yarn_ext_factor;
        case 3: return c.yarn_attn_factor;
        case 4: return c.yarn_beta_fast;
        case 5: return c.yarn_beta_slow;
        case 6: return h.f_norm_eps;
        case 7: return h.f_norm_rms_eps;
        default: GGML_ABORT("fg153_gh_fget: bad field");
    }
}

extern "C" void fg153_gc_clear(int64_t out, int64_t n) {
    GGML_ASSERT(out != 0 && n >= 0);
    std::memset((void *) (intptr_t) out, 0, (size_t) n);
}
extern "C" void fg153_gc_put(int64_t out, int64_t index, int64_t value) {
    GGML_ASSERT(out != 0 && index >= 0);
    ((int64_t *) (intptr_t) out)[index] = value;
}
extern "C" void fg153_gc_putf(int64_t out, int64_t index, float value) {
    // fg153_graph_meta has 29 int64 fields followed by the float block.
    GGML_ASSERT(out != 0 && index >= 0);
    float * base = (float *) ((unsigned char *) (intptr_t) out + 29 * sizeof(int64_t));
    base[index] = value;
}
