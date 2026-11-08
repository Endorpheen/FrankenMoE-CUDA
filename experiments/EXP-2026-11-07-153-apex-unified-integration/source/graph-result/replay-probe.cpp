// worker-7: end-to-end replay probe. The same source is linked once against
// the native libllama and once with patched/active-llama-graph.o (Goose hooks).
// Identical stdout proves the default-on replay and its native rollback mode.
#include "llama-graph.h"
#include "llama-hparams.h"
#include "llama-cparams.h"
#include "llama-batch.h"
#include "llama-adapter.h"
#include "llama-impl.h"

#include "ggml.h"
#include "ggml-backend.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line=%d %s\n", __LINE__, #x); std::abort(); } } while (0)

static uint64_t fnv(uint64_t h, const void * p, size_t n) {
    const uint8_t * b = (const uint8_t *) p;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}
static uint64_t mix(uint64_t h, int64_t v) { return fnv(h, &v, sizeof(v)); }

struct Ub {
    std::shared_ptr<llama_ubatch::data_t> d = std::make_shared<llama_ubatch::data_t>();
    llama_ubatch u {};
    Ub(uint32_t n) {
        d->token.resize(n);
        d->pos.resize(n);
        d->n_seq_id.assign(n, 1);
        d->seq_id_data.resize(n);
        d->seq_id.resize(n);
        for (uint32_t i = 0; i < n; ++i) {
            d->token[i] = (llama_token) (i + 1);
            d->pos[i] = (llama_pos) i;
            d->seq_id_data[i] = 0;
            d->seq_id[i] = &d->seq_id_data[i];
        }
        d->seq_id_unq = { 0 };
        d->seq_idx.assign(LLAMA_MAX_SEQ, -1);
        d->seq_idx[0] = 0;
        d->output.assign(n, 1);
        u.b_equal_seqs = 1; u.n_tokens = n; u.n_seq_tokens = n; u.n_seqs = 1;
        u.n_seqs_unq = 1; u.n_pos = 1;
        u.token = d->token.data(); u.pos = d->pos.data();
        u.n_seq_id = d->n_seq_id.data(); u.seq_id = d->seq_id.data();
        u.seq_id_unq = d->seq_id_unq.data(); u.seq_idx = d->seq_idx.data();
        u.output = d->output.data(); u.type = nullptr; u.data = d;
    }
};

int main() {
    llama_hparams hp {};
    hp.n_ctx_train = 128; hp.n_embd = 8; hp.n_layer_all = 3; hp.n_layer_nextn = 1;
    hp.n_embd_head_k_full = 8; hp.n_embd_head_v_full = 8; hp.n_rot_full = 8;
    hp.n_head_arr.fill(2); hp.n_head_kv_arr.fill(1); hp.n_ff_arr.fill(32);
    hp.n_ff_exp_arr.fill(32); hp.n_expert_used_arr.fill(2); hp.n_expert = 8;
    hp.f_norm_eps = 1e-5f; hp.f_norm_rms_eps = 1e-6f; hp.n_ctx_orig_yarn = 128;
    hp.rope_type = LLAMA_ROPE_TYPE_NORM;

    llama_cparams cp {};
    cp.n_ctx = 64; cp.n_ctx_seq = 64; cp.n_batch = 16; cp.n_ubatch = 16;
    cp.n_seq_max = 4; cp.n_outputs_max = 4; cp.n_outputs_max_per_seq = 1;
    cp.rope_freq_base = 10000.0f; cp.rope_freq_scale = 1.0f;
    cp.n_ctx_orig_yarn = 128; cp.yarn_ext_factor = -1.0f;
    cp.yarn_attn_factor = 1.0f; cp.yarn_beta_fast = 32.0f; cp.yarn_beta_slow = 1.0f;
    cp.causal_attn = true; cp.kv_unified = true; cp.pooling_type = LLAMA_POOLING_TYPE_NONE;
    cp.embeddings_layer_inp.assign(4, false);
    cp.embeddings_layer_inp[0] = true;
    cp.embeddings_layer_inp[2] = true;

    Ub ub(4);

    llm_graph_result r(64);
    auto * t = ggml_new_tensor_1d(r.get_ctx(), GGML_TYPE_F32, 4);
    auto * pos_t = ggml_new_tensor_1d(r.get_ctx(), GGML_TYPE_I32, 4);
    r.t_logits = t;
    r.t_embd = t;
    r.t_embd_pooled = t;
    r.t_h_nextn = t;
    r.t_layer_inp[0] = t;
    r.t_layer_inp[2] = t;
    r.t_sampled = { t };

    llm_graph_params p {};
    p.arch = LLM_ARCH_QWEN35MOE;
    p.hparams = hp;
    p.cparams = cp;
    p.ubatch = ub.u;
    p.gtype = LLM_GRAPH_TYPE_DEFAULT;
    p.n_outputs = 1;
    p.res = &r;

    r.set_outputs(p);
    auto pos = std::make_unique<llm_graph_input_pos>(1, LLAMA_ROPE_SECTION_ORDER_TYXZ);
    pos->pos = pos_t;
    llm_graph_input_pos * pos_raw = pos.get();
    CHECK(r.add_input(std::move(pos)) == pos_raw);
    r.add_fused_node({ LLM_FUSED_OP_FLASH_ATTN, t, 3 });
    r.set_hdmd_input(t, t, t);
    r.set_params(p);

    const bool reuse1 = r.can_reuse(p);
    llm_graph_params bad = p;
    bad.cparams.causal_attn = false;
    const bool reuse2 = r.can_reuse(bad);

    uint64_t h = 1469598103934665603ULL;
    h = mix(h, t->flags);
    h = mix(h, (int64_t) r.get_fused_nodes().size());
    h = mix(h, (int64_t) r.get_fused_nodes().at(0).op);
    h = mix(h, (int64_t) (intptr_t) r.get_fused_nodes().at(0).tensor == (int64_t) (intptr_t) t);
    h = mix(h, (int64_t) r.inputs.size());
    h = mix(h, (int64_t) (r.get_hdmd_input(t, t) == t));
    h = mix(h, r.get_max_nodes());
    h = mix(h, (int64_t) r.buf_compute_meta.size());
    h = mix(h, reuse1 ? 1 : 0);
    h = mix(h, reuse2 ? 1 : 0);

    r.reset();
    h = mix(h, r.get_logits() == nullptr);
    h = mix(h, (int64_t) r.inputs.size());
    h = mix(h, (int64_t) r.fused_nodes.size());
    h = mix(h, (int64_t) (r.get_hdmd_input(t, t) == nullptr));
    h = mix(h, (int64_t) r.buf_compute_meta.size());

    std::printf("%016llx reuse=%d/%d after=%zu/%zu\n",
                (unsigned long long) h, (int) reuse1, (int) reuse2,
                r.inputs.size(), r.fused_nodes.size());
    return 0;
}
