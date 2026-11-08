// worker-4/graph-metadata: synthetic real constructor corpus.
//
// Constructs real llama_hparams/llama_cparams/llama_ubatch/llm_graph_params and
// a real llm_graph_result (real ggml context/graph), then constructs the patched
// llm_graph_context and compares every field against the original derivation
// expressions. No model, no weights, no server.
#include "llama-graph.h"
#include "llama-hparams.h"
#include "llama-cparams.h"
#include "llama-batch.h"
#include "llama-adapter.h"
#include "llama-impl.h"
#include "graph-meta.h"

#include "ggml.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line=%d %s\n", __LINE__, #x); return 1; } } while (0)

struct Ub {
    std::shared_ptr<llama_ubatch::data_t> d = std::make_shared<llama_ubatch::data_t>();
    llama_ubatch u {};
    explicit Ub(uint32_t n) {
        d->token.resize(n); d->pos.resize(n); d->n_seq_id.assign(n, 1);
        d->seq_id_data.resize(n); d->seq_id.resize(n);
        for (uint32_t i = 0; i < n; ++i) {
            d->token[i] = (llama_token) (i + 1); d->pos[i] = (llama_pos) i;
            d->seq_id_data[i] = 0; d->seq_id[i] = &d->seq_id_data[i];
        }
        d->seq_id_unq = { 0 }; d->seq_idx.assign(LLAMA_MAX_SEQ, -1); d->seq_idx[0] = 0;
        d->output.assign(n, 1);
        u.b_equal_seqs = 1; u.n_tokens = n; u.n_seq_tokens = n; u.n_seqs = 1;
        u.n_seqs_unq = 1; u.n_pos = 1;
        u.token = d->token.data(); u.pos = d->pos.data(); u.n_seq_id = d->n_seq_id.data();
        u.seq_id = d->seq_id.data(); u.seq_id_unq = d->seq_id_unq.data();
        u.seq_idx = d->seq_idx.data(); u.output = d->output.data(); u.type = nullptr; u.data = d;
    }
};

static int run_case(const char * id, const llama_hparams & hp, const llama_cparams & cp,
                    uint32_t n_tokens, bool warmup) {
    llm_graph_params p {};
    p.arch = LLM_ARCH_QWEN35MOE;
    p.hparams = hp;
    p.cparams = cp;
    p.cparams.warmup = warmup;
    Ub ub(n_tokens);
    p.ubatch = ub.u;
    p.gtype = LLM_GRAPH_TYPE_DEFAULT;
    p.n_outputs = 2;
    p.sched = (ggml_backend_sched_t) (intptr_t) 0x1000;
    p.backend_cpu = (ggml_backend_t) (intptr_t) 0x2000;
    p.cvec = (const llama_adapter_cvec *) (intptr_t) 0x3000;
    p.loras = (const llama_adapter_loras *) (intptr_t) 0x4000;
    p.mctx = (const llama_memory_context_i *) (intptr_t) 0x5000;
    p.cross = (const llama_cross *) (intptr_t) 0x6000;
    p.moe_cache = (const llama_moe_cache *) (intptr_t) 0x7000;
    p.prec_policy = (const llama_prec_policy *) (intptr_t) 0x8000;
    p.hdmd = (const llama_hadamard *) (intptr_t) 0x9000;

    llm_graph_result r(64);
    p.res = &r;

    llm_graph_context ctx(p);

    const int64_t want_i[] = {
        (int64_t) p.arch, (int64_t) hp.n_embd, (int64_t) hp.n_layer(), (int64_t) hp.n_layer_nextn,
        (int64_t) hp.n_rot(), (int64_t) cp.n_ctx, (int64_t) hp.n_head(), (int64_t) hp.n_head_kv(),
        (int64_t) hp.n_embd_head_k(), (int64_t) hp.n_embd_k_gqa(), (int64_t) hp.n_embd_head_v(),
        (int64_t) hp.n_embd_v_gqa(), (int64_t) hp.n_expert,
        (int64_t) (cp.warmup ? hp.n_expert : hp.n_expert_used()),
        (int64_t) p.ubatch.n_tokens, (int64_t) p.n_outputs, (int64_t) cp.n_ctx_orig_yarn,
        (int64_t) cp.pooling_type, (int64_t) hp.rope_type,
        (int64_t) (intptr_t) p.sched, (int64_t) (intptr_t) p.backend_cpu,
        (int64_t) (intptr_t) p.cvec, (int64_t) (intptr_t) p.loras,
        (int64_t) (intptr_t) p.mctx, (int64_t) (intptr_t) p.cross,
        (int64_t) (intptr_t) p.moe_cache, (int64_t) (intptr_t) p.prec_policy,
        (int64_t) (intptr_t) p.hdmd, (int64_t) (intptr_t) p.res,
    };
    const int64_t got_i[] = {
        (int64_t) ctx.arch, ctx.n_embd, ctx.n_layer, ctx.n_layer_nextn, ctx.n_rot, ctx.n_ctx,
        ctx.n_head, ctx.n_head_kv, ctx.n_embd_head_k, ctx.n_embd_k_gqa, ctx.n_embd_head_v,
        ctx.n_embd_v_gqa, ctx.n_expert, ctx.n_expert_used, ctx.n_tokens, ctx.n_outputs,
        ctx.n_ctx_orig, (int64_t) ctx.pooling_type, (int64_t) ctx.rope_type,
        (int64_t) (intptr_t) ctx.sched, (int64_t) (intptr_t) ctx.backend_cpu,
        (int64_t) (intptr_t) ctx.cvec, (int64_t) (intptr_t) ctx.loras,
        (int64_t) (intptr_t) ctx.mctx, (int64_t) (intptr_t) ctx.cross,
        (int64_t) (intptr_t) ctx.moe_cache, (int64_t) (intptr_t) ctx.prec_policy,
        (int64_t) (intptr_t) ctx.hdmd, (int64_t) (intptr_t) ctx.res,
    };
    const float want_f[] = {
        cp.rope_freq_base, cp.rope_freq_scale, cp.yarn_ext_factor, cp.yarn_attn_factor,
        cp.yarn_beta_fast, cp.yarn_beta_slow, hp.f_norm_eps, hp.f_norm_rms_eps,
    };
    const float got_f[] = {
        ctx.freq_base, ctx.freq_scale, ctx.ext_factor, ctx.attn_factor,
        ctx.beta_fast, ctx.beta_slow, ctx.norm_eps, ctx.norm_rms_eps,
    };
    for (size_t i = 0; i < sizeof(want_i)/sizeof(want_i[0]); ++i) {
        if (want_i[i] != got_i[i]) {
            std::fprintf(stderr, "%s: int field %zu want=%lld got=%lld\n", id, i,
                         (long long) want_i[i], (long long) got_i[i]);
            return 1;
        }
    }
    for (size_t i = 0; i < sizeof(want_f)/sizeof(want_f[0]); ++i) {
        if (want_f[i] != got_f[i]) {
            std::fprintf(stderr, "%s: float field %zu want=%g got=%g\n", id, i, want_f[i], got_f[i]);
            return 1;
        }
    }
    if (&ctx.hparams != &p.hparams || &ctx.cparams != &p.cparams || &ctx.ubatch != &p.ubatch) {
        std::fprintf(stderr, "%s: const reference members not bound to params\n", id);
        return 1;
    }
    if (&ctx.cb_func != &p.cb) {
        std::fprintf(stderr, "%s: cb_func reference not bound to params.cb\n", id);
        return 1;
    }
    if (ctx.ctx0 != r.get_ctx() || ctx.gf != r.get_gf()) {
        std::fprintf(stderr, "%s: ctx0/gf not projected from result\n", id);
        return 1;
    }
    std::printf("PASS %s\n", id);
    return 0;
}

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
    cp.warmup = false;

    if (run_case("apex_like", hp, cp, 4, false) != 0) return 1;

    llama_hparams hp2 = hp;
    hp2.n_embd = 16; hp2.n_layer_all = 5; hp2.n_layer_nextn = 0;
    hp2.n_embd_head_k_full = 4; hp2.n_embd_head_v_full = 4; hp2.n_rot_full = 4;
    hp2.n_head_arr.fill(4); hp2.n_head_kv_arr.fill(2);
    hp2.n_expert = 0; hp2.n_expert_used_arr.fill(0);
    hp2.rope_type = LLAMA_ROPE_TYPE_NEOX;
    llama_cparams cp2 = cp;
    cp2.n_ctx = 96; cp2.pooling_type = LLAMA_POOLING_TYPE_MEAN;
    cp2.rope_freq_base = 500000.0f; cp2.rope_freq_scale = 0.5f;
    cp2.yarn_ext_factor = 0.25f; cp2.yarn_attn_factor = 1.5f;
    if (run_case("gqa_nextn0_warmup", hp2, cp2, 7, true) != 0) return 1;

    std::printf("{\"status\":\"PASS\",\"corpus\":\"fg153-graph-metadata-ctor\"}\n");
    return 0;
}
