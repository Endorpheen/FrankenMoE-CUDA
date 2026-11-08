// worker-7: real native-vs-Goose executable corpus for the graph-result/input
// block. Uses real ggml contexts, real tensors, real CPU backend buffers and
// real llama_ubatch/llm_graph_params values. No model, weights or inference.
#include "graph-bridge.h"

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
    for (size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t mix(uint64_t h, int64_t v) {
    return fnv(h, &v, sizeof(v));
}

static uint64_t hash_tensor(ggml_tensor * t) {
    if (!t) {
        return 0x9e3779b97f4a7c15ULL;
    }
    uint64_t h = 1469598103934665603ULL;
    h = mix(h, t->type);
    h = mix(h, t->op);
    h = mix(h, t->flags);
    for (int i = 0; i < 4; ++i) {
        h = mix(h, t->ne[i]);
        h = mix(h, (int64_t) t->nb[i]);
    }
    const size_t n = ggml_nbytes(t);
    if (n > 0 && t->buffer) {
        std::vector<unsigned char> bytes(n);
        ggml_backend_tensor_get(t, bytes.data(), 0, n);
        h = fnv(h, bytes.data(), bytes.size());
    }
    return h;
}

struct Ub {
    std::shared_ptr<llama_ubatch::data_t> d = std::make_shared<llama_ubatch::data_t>();
    llama_ubatch u {};

    Ub(uint32_t n_tokens, uint32_t n_seq_tokens, uint32_t n_seqs,
       bool with_token, bool with_embd, bool mixed, bool with_state,
       uint32_t n_pos, bool equal_seqs, bool all_output = true)
        : u() {
        d->token.resize(with_token || mixed ? n_tokens : 0);
        for (size_t i = 0; i < d->token.size(); ++i) {
            d->token[i] = (llama_token) (i + 1);
        }
        d->embd.resize(with_embd || mixed ? (size_t) n_tokens * 8 : 0);
        for (size_t i = 0; i < d->embd.size(); ++i) {
            d->embd[i] = (float) i * 0.25f;
        }
        d->embd_state.resize(with_state ? (size_t) n_tokens * 3 : 0);
        for (size_t i = 0; i < d->embd_state.size(); ++i) {
            d->embd_state[i] = (float) i * 0.5f;
        }
        d->pos.resize((size_t) n_tokens * n_pos);
        for (uint32_t p = 0; p < n_pos; ++p) {
            for (uint32_t i = 0; i < n_tokens; ++i) {
                d->pos[(size_t) p * n_tokens + i] = (llama_pos) (i + p * 1000);
            }
        }
        d->n_seq_id.assign(n_tokens, 1);
        d->seq_id_data.resize(n_tokens);
        d->seq_id.resize(n_tokens);
        for (uint32_t i = 0; i < n_tokens; ++i) {
            d->seq_id_data[i] = (llama_seq_id) (i % n_seqs);
            d->seq_id[i] = &d->seq_id_data[i];
        }
        d->seq_id_unq.resize(n_seqs);
        d->seq_idx.assign(LLAMA_MAX_SEQ, -1);
        for (uint32_t s = 0; s < n_seqs; ++s) {
            d->seq_id_unq[s] = (llama_seq_id) s;
            d->seq_idx[s] = (int32_t) s;
        }
        d->output.assign(n_tokens, all_output ? 1 : 0);
        d->type.assign(mixed ? n_tokens : 0, 0);
        for (uint32_t i = 0; i < n_tokens; ++i) {
            if (mixed) {
                d->type[i] = (int8_t) (i & 1);
            }
        }

        u.b_equal_seqs   = equal_seqs ? 1 : 0;
        u.n_tokens       = n_tokens;
        u.n_seq_tokens   = n_seq_tokens;
        u.n_seqs         = n_seqs;
        u.n_seqs_unq     = n_seqs;
        u.n_pos          = n_pos;
        u.token          = (with_token || mixed) ? d->token.data() : nullptr;
        u.embd           = (with_embd || mixed) ? d->embd.data() : nullptr;
        u.embd_state     = with_state ? d->embd_state.data() : nullptr;
        u.pos            = d->pos.data();
        u.n_seq_id       = d->n_seq_id.data();
        u.seq_id         = d->seq_id.data();
        u.seq_id_unq     = d->seq_id_unq.data();
        u.seq_idx        = d->seq_idx.data();
        u.output         = d->output.data();
        u.type           = mixed ? d->type.data() : nullptr;
        u.data           = d;
    }
};

static llama_hparams make_hparams() {
    llama_hparams h {};
    h.n_ctx_train = 512;
    h.n_embd = 8;
    h.n_layer_all = 3;
    h.n_layer_nextn = 1;
    h.n_embd_head_k_full = 8;
    h.n_embd_head_v_full = 8;
    h.n_rot_full = 8;
    h.n_head_arr.fill(2);
    h.n_head_kv_arr.fill(1);
    h.n_ff_arr.fill(32);
    h.n_ff_exp_arr.fill(32);
    h.n_expert_used_arr.fill(2);
    h.n_expert = 8;
    h.f_norm_eps = 1e-5f;
    h.f_norm_rms_eps = 1e-6f;
    h.n_ctx_orig_yarn = 512;
    h.rope_type = LLAMA_ROPE_TYPE_NORM;
    return h;
}

static llama_cparams make_cparams() {
    llama_cparams c {};
    c.n_ctx = 128;
    c.n_ctx_seq = 128;
    c.n_batch = 32;
    c.n_ubatch = 32;
    c.n_seq_max = 4;
    c.n_outputs_max = 8;
    c.n_outputs_max_per_seq = 1;
    c.rope_freq_base = 10000.0f;
    c.rope_freq_scale = 1.0f;
    c.n_ctx_orig_yarn = 512;
    c.yarn_ext_factor = -1.0f;
    c.yarn_attn_factor = 1.0f;
    c.yarn_beta_fast = 32.0f;
    c.yarn_beta_slow = 1.0f;
    c.embeddings = false;
    c.causal_attn = true;
    c.kv_unified = true;
    c.flash_attn = false;
    c.pooling_type = LLAMA_POOLING_TYPE_NONE;
    c.embeddings_layer_inp.assign(4, false);
    return c;
}

struct Ctx {
    ggml_context * ctx = nullptr;
    ggml_backend_buffer_t buf = nullptr;
    Ctx() {
        ggml_init_params ip = { (size_t) 4 * 1024 * 1024, nullptr, true };
        ctx = ggml_init(ip);
        CHECK(ctx != nullptr);
    }
    ~Ctx() {
        if (buf) ggml_backend_buffer_free(buf);
        if (ctx) ggml_free(ctx);
    }
    void alloc() {
        buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx, ggml_backend_cpu_buffer_type());
        CHECK(buf != nullptr);
        ggml_backend_buffer_clear(buf, 0);
    }
};

struct EmbdTensors {
    ggml_tensor * tokens = nullptr;
    ggml_tensor * embd = nullptr;
    ggml_tensor * h = nullptr;
    ggml_tensor * mixed_tokens = nullptr;
    ggml_tensor * mixed_slots = nullptr;
    ggml_tensor * mixed_embd = nullptr;
    ggml_tensor * scale_rows = nullptr;
};

static EmbdTensors make_embd_tensors(ggml_context * ctx, int64_t n_embd, int64_t n_tokens,
                                     int64_t rows, bool with_token, bool with_embd, bool mixed) {
    EmbdTensors t;
    if (with_token || mixed) {
        t.tokens = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, n_tokens);
    }
    if (with_embd || mixed) {
        t.embd = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, n_embd, n_tokens);
    }
    if (mixed) {
        t.mixed_tokens = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, rows);
        t.mixed_slots = ggml_new_tensor_1d(ctx, GGML_TYPE_I64, rows);
        t.mixed_embd = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, n_embd, n_tokens);
    }
    t.scale_rows = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 1, n_tokens);
    return t;
}

static void bind_embd(llm_graph_input_embd & in, const EmbdTensors & t, float scale) {
    in.tokens = t.tokens;
    in.embd = t.embd;
    in.mixed_tokens = t.mixed_tokens;
    in.mixed_slots = t.mixed_slots;
    in.mixed_embd = t.mixed_embd;
    in.scale_rows = t.scale_rows;
    in.scale_tok = scale;
}

static uint64_t hash_embd(const EmbdTensors & t) {
    uint64_t h = 1469598103934665603ULL;
    h = mix(h, hash_tensor(t.tokens));
    h = mix(h, hash_tensor(t.embd));
    h = mix(h, hash_tensor(t.mixed_tokens));
    h = mix(h, hash_tensor(t.mixed_slots));
    h = mix(h, hash_tensor(t.mixed_embd));
    h = mix(h, hash_tensor(t.scale_rows));
    return h;
}

static uint64_t embd_case(const llm_graph_params & p, const Ub & ub, int64_t n_embd,
                          bool with_token, bool with_embd, bool mixed, float scale,
                          int64_t rows_override = -1) {
    (void) p;
    const int64_t n = ub.u.n_tokens;
    const int64_t rows = rows_override >= 0 ? rows_override : n;
    Ctx c;
    const EmbdTensors a = make_embd_tensors(c.ctx, n_embd, n, rows, with_token, with_embd, mixed);
    const EmbdTensors b = make_embd_tensors(c.ctx, n_embd, n, rows, with_token, with_embd, mixed);
    c.alloc();

    llm_graph_input_embd na((int64_t) n_embd), ga((int64_t) n_embd);
    bind_embd(na, a, scale);
    bind_embd(ga, b, scale);
    na.set_input(&ub.u);
    fg153_goose_inp(0, (int64_t) (intptr_t) &ga, (int64_t) (intptr_t) &ub.u, 0, 0, 0);

    CHECK(hash_embd(a) == hash_embd(b));
    return hash_embd(a);
}

static void embd_reuse_case(const llm_graph_params & p, const llm_graph_params & other,
                            int64_t n_embd, bool with_token, bool with_embd, bool mixed,
                            int64_t rows, bool expect) {
    Ctx c;
    const EmbdTensors t = make_embd_tensors(c.ctx, n_embd, p.ubatch.n_tokens, rows,
                                            with_token, with_embd, mixed);
    llm_graph_input_embd n((int64_t) n_embd), g((int64_t) n_embd);
    bind_embd(n, t, 1.0f);
    bind_embd(g, t, 1.0f);
    const bool native = n.can_reuse(other);
    const bool goose = fg153_goose_inp(1, (int64_t) (intptr_t) &g,
                                      (int64_t) (intptr_t) &other, 0, 0, 0) != 0;
    CHECK(native == goose);
    CHECK(native == expect);
}

struct PosTensors {
    ggml_tensor * pos = nullptr;
};

static uint64_t pos_case(const llm_graph_params & p, const Ub & ub, int64_t npe,
                         llama_rope_section_order order) {
    (void) p;
    Ctx c;
    auto * a = ggml_new_tensor_1d(c.ctx, GGML_TYPE_I32, ub.u.n_tokens * npe);
    auto * b = ggml_new_tensor_1d(c.ctx, GGML_TYPE_I32, ub.u.n_tokens * npe);
    c.alloc();
    llm_graph_input_pos n((uint32_t) npe, order), g((uint32_t) npe, order);
    n.pos = a;
    g.pos = b;
    n.set_input(&ub.u);
    fg153_goose_inp(4, (int64_t) (intptr_t) &g, (int64_t) (intptr_t) &ub.u, 0, 0, 0);
    CHECK(hash_tensor(a) == hash_tensor(b));
    return hash_tensor(a);
}

static void pos_reuse_case(const llm_graph_params & p, const llm_graph_params & other,
                           int64_t npe, bool expect) {
    Ctx c;
    auto * a = ggml_new_tensor_1d(c.ctx, GGML_TYPE_I32, p.ubatch.n_tokens * npe);
    llm_graph_input_pos n((uint32_t) npe, LLAMA_ROPE_SECTION_ORDER_TYXZ);
    llm_graph_input_pos g((uint32_t) npe, LLAMA_ROPE_SECTION_ORDER_TYXZ);
    n.pos = a;
    g.pos = a;
    const bool native = n.can_reuse(other);
    const bool goose = fg153_goose_inp(5, (int64_t) (intptr_t) &g,
                                      (int64_t) (intptr_t) &other, 0, 0, 0) != 0;
    CHECK(native == goose);
    CHECK(native == expect);
}

static uint64_t out_ids_case(const llm_graph_params & p, const Ub & ub, int64_t nout) {
    Ctx c;
    auto * a = ggml_new_tensor_1d(c.ctx, GGML_TYPE_I32, nout);
    auto * b = ggml_new_tensor_1d(c.ctx, GGML_TYPE_I32, nout);
    c.alloc();
    llm_graph_input_out_ids n(p.hparams, p.cparams, (uint32_t) nout), g(p.hparams, p.cparams, (uint32_t) nout);
    n.out_ids = a;
    g.out_ids = b;
    n.set_input(&ub.u);
    fg153_goose_inp(6, (int64_t) (intptr_t) &g, (int64_t) (intptr_t) &ub.u, 0, 0, 0);
    CHECK(hash_tensor(a) == hash_tensor(b));
    return hash_tensor(a);
}

static void out_ids_reuse_case(const llm_graph_params & p, const llm_graph_params & other,
                               int64_t nout, bool expect) {
    Ctx c;
    auto * a = ggml_new_tensor_1d(c.ctx, GGML_TYPE_I32, nout);
    llm_graph_input_out_ids n(p.hparams, p.cparams, (uint32_t) nout);
    llm_graph_input_out_ids g(p.hparams, p.cparams, (uint32_t) nout);
    n.out_ids = a;
    g.out_ids = a;
    const bool native = n.can_reuse(other);
    const bool goose = fg153_goose_inp(7, (int64_t) (intptr_t) &g,
                                      (int64_t) (intptr_t) &other, 0, 0, 0) != 0;
    CHECK(native == goose);
    CHECK(native == expect);
}

static llm_graph_params make_params(const llama_hparams & hp, const llama_cparams & cp,
                                    const llama_ubatch & ub, llm_graph_result * res,
                                    uint32_t n_outputs, llm_arch arch = LLM_ARCH_QWEN35MOE) {
    llm_graph_params p {};
    p.arch = arch;
    p.hparams = hp;
    p.cparams = cp;
    p.ubatch = ub;
    p.gtype = LLM_GRAPH_TYPE_DEFAULT;
    p.n_outputs = n_outputs;
    p.res = res;
    return p;
}

static void result_lifecycle() {
    const llama_hparams hp = make_hparams();
    const llama_cparams cp = make_cparams();
    Ub ub(4, 4, 1, true, false, false, false, 1, true);

    llm_graph_result nr(128);
    llm_graph_result gr(128);
    llm_graph_params stored {};
    CHECK(fg153_goose_res(0, (int64_t) (intptr_t) &gr, 128, (int64_t) (intptr_t) &stored, 0, 0) == 0);

    CHECK(nr.get_max_nodes() == 128);
    CHECK(gr.get_max_nodes() == 128);
    CHECK(nr.get_ctx() != nullptr && gr.get_ctx() != nullptr);
    CHECK(nr.get_gf() != nullptr && gr.get_gf() != nullptr);
    CHECK(nr.get_logits() == nullptr && gr.get_logits() == nullptr);
    CHECK(nr.inputs.empty() && gr.inputs.empty());
    CHECK(nr.fused_nodes.empty() && gr.fused_nodes.empty());
    CHECK(nr.buf_compute_meta.size() == gr.buf_compute_meta.size());

    llm_graph_params p = make_params(hp, cp, ub.u, &nr, 3);
    p.cparams.embeddings_layer_inp.assign(4, false);
    p.cparams.embeddings_layer_inp[0] = true;
    p.cparams.embeddings_layer_inp[2] = true;

    auto * nl = ggml_new_tensor_1d(nr.get_ctx(), GGML_TYPE_F32, 5);
    auto * ne = ggml_new_tensor_1d(nr.get_ctx(), GGML_TYPE_F32, 5);
    auto * np = ggml_new_tensor_1d(nr.get_ctx(), GGML_TYPE_F32, 5);
    auto * gl = ggml_new_tensor_1d(gr.get_ctx(), GGML_TYPE_F32, 5);
    auto * ge = ggml_new_tensor_1d(gr.get_ctx(), GGML_TYPE_F32, 5);
    auto * gp = ggml_new_tensor_1d(gr.get_ctx(), GGML_TYPE_F32, 5);
    nr.t_logits = nl;
    nr.t_embd = ne;
    nr.t_embd_pooled = np;
    nr.t_layer_inp[0] = nl;
    nr.t_layer_inp[2] = np;
    nr.t_sampled = { nl };
    nr.t_sampled_probs = { ne };
    nr.t_sampled_logits = { np };
    nr.t_candidates = { nl };
    gr.t_logits = gl;
    gr.t_embd = ge;
    gr.t_embd_pooled = gp;
    gr.t_layer_inp[0] = gl;
    gr.t_layer_inp[2] = gp;
    gr.t_sampled = { gl };
    gr.t_sampled_probs = { ge };
    gr.t_sampled_logits = { gp };
    gr.t_candidates = { gl };

    nr.set_outputs(p);
    CHECK(fg153_goose_res(2, (int64_t) (intptr_t) &gr, (int64_t) (intptr_t) &p, 0, 0, 0) == 0);
    const int64_t outs[] = { (int64_t) (nl->flags & GGML_TENSOR_FLAG_OUTPUT),
                             (int64_t) (ne->flags & GGML_TENSOR_FLAG_OUTPUT),
                             (int64_t) (np->flags & GGML_TENSOR_FLAG_OUTPUT) };
    const int64_t gout[] = { (int64_t) (gl->flags & GGML_TENSOR_FLAG_OUTPUT),
                             (int64_t) (ge->flags & GGML_TENSOR_FLAG_OUTPUT),
                             (int64_t) (gp->flags & GGML_TENSOR_FLAG_OUTPUT) };
    for (int i = 0; i < 3; ++i) {
        CHECK(outs[i] == gout[i]);
        CHECK(outs[i] == GGML_TENSOR_FLAG_OUTPUT);
    }

    nr.add_fused_node({ LLM_FUSED_OP_FLASH_ATTN, nl, 3 });
    CHECK(fg153_goose_res(5, (int64_t) (intptr_t) &gr, LLM_FUSED_OP_FLASH_ATTN,
                          (int64_t) (intptr_t) gl, 3, 0) == 0);
    CHECK(nr.get_fused_nodes().size() == 1);
    CHECK(gr.get_fused_nodes().size() == 1);
    CHECK(nr.get_fused_nodes()[0].op == gr.get_fused_nodes()[0].op);
    CHECK(nr.get_fused_nodes()[0].il == gr.get_fused_nodes()[0].il);

    nr.set_hdmd_input(ne, np, nl);
    CHECK(fg153_goose_res(7, (int64_t) (intptr_t) &gr, (int64_t) (intptr_t) ge,
                          (int64_t) (intptr_t) gp, (int64_t) (intptr_t) gl, 0) == 0);
    CHECK(nr.get_hdmd_input(ne, np) == nl);
    CHECK(gr.get_hdmd_input(ge, gp) == gl);

    auto npos = std::make_unique<llm_graph_input_pos>(1, LLAMA_ROPE_SECTION_ORDER_TYXZ);
    npos->pos = ggml_new_tensor_1d(nr.get_ctx(), GGML_TYPE_I32, 4);
    llm_graph_input_pos * nrp = npos.get();
    CHECK(nr.add_input(std::move(npos)) == nrp);
    auto * gpos = new llm_graph_input_pos(1, LLAMA_ROPE_SECTION_ORDER_TYXZ);
    gpos->pos = ggml_new_tensor_1d(gr.get_ctx(), GGML_TYPE_I32, 4);
    CHECK(fg153_goose_res(4, (int64_t) (intptr_t) &gr,
                          (int64_t) (intptr_t) gpos, 0, 0, 0) != 0);
    CHECK(nr.inputs.size() == 1 && gr.inputs.size() == 1);

    llm_graph_params p0 = make_params(hp, cp, ub.u, &nr, 3);
    llm_graph_params p1 = p0;
    nr.set_params(p0);
    CHECK(fg153_goose_res(8, (int64_t) (intptr_t) &gr, (int64_t) (intptr_t) &p0,
                          (int64_t) (intptr_t) &stored, 0, 0) == 0);
    CHECK(nr.can_reuse(p1) == (fg153_goose_res(3, (int64_t) (intptr_t) &gr,
        (int64_t) (intptr_t) &p1, (int64_t) (intptr_t) &stored, 2, 0) != 0));
    CHECK(nr.can_reuse(p1));

    llm_graph_params bad = p1;
    bad.ubatch.n_tokens += 1;
    CHECK(!nr.can_reuse(bad));
    CHECK(fg153_goose_res(3, (int64_t) (intptr_t) &gr, (int64_t) (intptr_t) &bad,
                          (int64_t) (intptr_t) &stored, 2, 0) == 0);

    llm_graph_params bad2 = p1;
    bad2.cparams.causal_attn = false;
    CHECK(!nr.can_reuse(bad2));
    CHECK(fg153_goose_res(3, (int64_t) (intptr_t) &gr, (int64_t) (intptr_t) &bad2,
                          (int64_t) (intptr_t) &stored, 2, 0) == 0);

    llm_graph_params bad3 = p1;
    bad3.arch = LLM_ARCH_LLAMA;
    CHECK(!nr.can_reuse(bad3));
    CHECK(fg153_goose_res(3, (int64_t) (intptr_t) &gr, (int64_t) (intptr_t) &bad3,
                          (int64_t) (intptr_t) &stored, 2, 0) == 0);

    nr.reset();
    CHECK(fg153_goose_res(0, (int64_t) (intptr_t) &gr, 128, (int64_t) (intptr_t) &stored, 0, 0) == 0);
    CHECK(stored.res == nullptr && stored.n_outputs == 0);
    CHECK(nr.get_logits() == nullptr && gr.get_logits() == nullptr);
    CHECK(nr.get_embd() == nullptr && gr.get_embd() == nullptr);
    CHECK(nr.inputs.empty() && gr.inputs.empty());
    CHECK(nr.fused_nodes.empty() && gr.fused_nodes.empty());
    CHECK(nr.get_hdmd_input(ne, np) == nullptr && gr.get_hdmd_input(ge, gp) == nullptr);
    for (int i = 0; i < LLAMA_MAX_LAYERS + 1; ++i) {
        CHECK(nr.get_layer_inp(i) == nullptr);
        CHECK(gr.get_layer_inp(i) == nullptr);
    }
    CHECK(nr.buf_compute_meta.size() == gr.buf_compute_meta.size());

    // Ownership stress: repeated add/reset cycles must destroy every owned
    // input and clear every container without stale handles.
    for (int cycle = 0; cycle < 8; ++cycle) {
        auto * np = new llm_graph_input_pos(1, LLAMA_ROPE_SECTION_ORDER_TYXZ);
        np->pos = ggml_new_tensor_1d(nr.get_ctx(), GGML_TYPE_I32, 4);
        auto * gp = new llm_graph_input_pos(1, LLAMA_ROPE_SECTION_ORDER_TYXZ);
        gp->pos = ggml_new_tensor_1d(gr.get_ctx(), GGML_TYPE_I32, 4);
        CHECK(nr.add_input(std::unique_ptr<llm_graph_input_i>(np)) != nullptr);
        CHECK(fg153_goose_res(4, (int64_t) (intptr_t) &gr, (int64_t) (intptr_t) gp, 0, 0, 0) != 0);
        nr.add_fused_node({ LLM_FUSED_OP_GDN_AR, np->pos, cycle });
        CHECK(fg153_goose_res(5, (int64_t) (intptr_t) &gr, LLM_FUSED_OP_GDN_AR,
                              (int64_t) (intptr_t) gp->pos, cycle, 0) == 0);
        CHECK(nr.inputs.size() == 1 && gr.inputs.size() == 1);
        CHECK(nr.fused_nodes.size() == 1 && gr.fused_nodes.size() == 1);
        nr.reset();
        CHECK(fg153_goose_res(0, (int64_t) (intptr_t) &gr, 128, (int64_t) (intptr_t) &stored, 0, 0) == 0);
        CHECK(nr.inputs.empty() && gr.inputs.empty());
        CHECK(nr.fused_nodes.empty() && gr.fused_nodes.empty());
    }

    std::puts("PASS result lifecycle/reset/outputs/fused/hdmd/ownership/can_reuse");
}

static void input_corpus() {
    const llama_hparams hp = make_hparams();
    const llama_cparams cp = make_cparams();
    llm_graph_result res(64);

    {
        Ub ub(6, 6, 1, true, false, false, false, 1, true);
        llm_graph_params p = make_params(hp, cp, ub.u, &res, 6);
        embd_case(p, ub, 8, true, false, false, 0.5f);

        Ub embd_only(6, 6, 1, false, true, false, false, 1, true);
        llm_graph_params pe = make_params(hp, cp, embd_only.u, &res, 6);
        embd_case(pe, embd_only, 8, false, true, false, 0.5f);

        Ub mixed(6, 6, 1, true, true, true, false, 1, true);
        llm_graph_params pm = make_params(hp, cp, mixed.u, &res, 6);
        embd_case(pm, mixed, 8, true, true, true, 0.5f, 3);

        Ub st(6, 6, 1, true, false, false, true, 1, true);
        llm_graph_params ps = make_params(hp, cp, st.u, &res, 6);
        Ctx c;
        EmbdTensors a, b;
        a.tokens = ggml_new_tensor_1d(c.ctx, GGML_TYPE_I32, 6);
        a.h = ggml_new_tensor_2d(c.ctx, GGML_TYPE_F32, 3, 6);
        b.tokens = ggml_new_tensor_1d(c.ctx, GGML_TYPE_I32, 6);
        b.h = ggml_new_tensor_2d(c.ctx, GGML_TYPE_F32, 3, 6);
        c.alloc();
        llm_graph_input_embd_h n(8, 3), g(8, 3);
        n.tokens = a.tokens; n.h = a.h;
        g.tokens = b.tokens; g.h = b.h;
        n.set_input(&st.u);
        fg153_goose_inp(2, (int64_t) (intptr_t) &g, (int64_t) (intptr_t) &st.u, 0, 0, 0);
        CHECK(hash_tensor(a.tokens) == hash_tensor(b.tokens));
        CHECK(hash_tensor(a.h) == hash_tensor(b.h));

        // can_reuse: matching, n_tokens mismatch, mixed row mismatch, malformed shape.
        embd_reuse_case(p, p, 8, true, false, false, 6, true);
        llm_graph_params bad = p;
        bad.ubatch.n_tokens += 1;
        embd_reuse_case(p, bad, 8, true, false, false, 6, false);
        embd_reuse_case(pm, pm, 8, true, true, true, 3, true);
        embd_reuse_case(pm, bad, 8, true, true, true, 3, false);
        // malformed tensor bound: token tensor one element too short.
        {
            Ctx c2;
            auto * tok = ggml_new_tensor_1d(c2.ctx, GGML_TYPE_I32, 5);
            llm_graph_input_embd n2(8), g2(8);
            n2.tokens = tok; n2.embd = nullptr; n2.mixed_tokens = nullptr;
            n2.mixed_slots = nullptr; n2.mixed_embd = nullptr; n2.scale_rows = nullptr;
            g2.tokens = tok; g2.embd = nullptr; g2.mixed_tokens = nullptr;
            g2.mixed_slots = nullptr; g2.mixed_embd = nullptr; g2.scale_rows = nullptr;
            CHECK(!n2.can_reuse(p));
            CHECK(fg153_goose_inp(1, (int64_t) (intptr_t) &g2, (int64_t) (intptr_t) &p, 0, 0, 0) == 0);
        }

        // position inputs: TYXZ and ZYXT, dense and mixed.
        pos_case(p, ub, 1, LLAMA_ROPE_SECTION_ORDER_TYXZ);
        Ub m4(6, 6, 1, true, true, true, false, 4, true);
        llm_graph_params p4 = make_params(hp, cp, m4.u, &res, 6);
        pos_case(p4, m4, 4, LLAMA_ROPE_SECTION_ORDER_ZYXT);
        pos_case(p4, m4, 4, LLAMA_ROPE_SECTION_ORDER_TYXZ);
        pos_reuse_case(p, p, 1, true);
        pos_reuse_case(p, bad, 1, false);
        {
            Ctx c3;
            auto * wrong = ggml_new_tensor_1d(c3.ctx, GGML_TYPE_I32, 5);
            llm_graph_input_pos n3(1), g3(1);
            n3.pos = wrong; g3.pos = wrong;
            CHECK(!n3.can_reuse(p));
            CHECK(fg153_goose_inp(5, (int64_t) (intptr_t) &g3, (int64_t) (intptr_t) &p, 0, 0, 0) == 0);
        }

        // output ids: all rows, subset, and a malformed n_outputs bound.
        out_ids_case(p, ub, 6);
        Ub out_sub(6, 6, 1, true, false, false, false, 1, true);
        for (uint32_t i = 0; i < 6; ++i) {
            out_sub.d->output[i] = (i % 2) == 0;
        }
        llm_graph_params pos3 = make_params(hp, cp, out_sub.u, &res, 3);
        out_ids_case(pos3, out_sub, 3);
        out_ids_reuse_case(pos3, pos3, 3, true);
        llm_graph_params bad_out = pos3;
        bad_out.n_outputs = 4;
        out_ids_reuse_case(pos3, bad_out, 3, false);
        {
            Ctx c4;
            auto * ids = ggml_new_tensor_1d(c4.ctx, GGML_TYPE_I32, 3);
            llm_graph_input_out_ids n4(hp, cp, 3), g4(hp, cp, 3);
            n4.out_ids = ids; g4.out_ids = ids;
            CHECK(!n4.can_reuse(bad_out));
            CHECK(fg153_goose_inp(7, (int64_t) (intptr_t) &g4, (int64_t) (intptr_t) &bad_out, 0, 0, 0) == 0);
        }
    }

    std::puts("PASS real CPU input buffers: tokens/embd/mixed/embd_h/pos/out_ids and reuse");
}

static void context_metadata() {
    const llama_hparams hp = make_hparams();
    const llama_cparams cp = make_cparams();
    Ub ub(5, 5, 1, true, false, false, false, 1, true);
    llm_graph_result res(64);
    llm_graph_params p = make_params(hp, cp, ub.u, &res, 5);
    p.cparams.embeddings_layer_inp.assign(4, false);

    llm_graph_context native(p);
    fg153_graph_meta m {};
    CHECK(fg153_goose_ctx(0, (int64_t) (intptr_t) &p, (int64_t) (intptr_t) &m, 0, 0, 0) == 0);

    CHECK(m.arch == (int64_t) native.arch);
    CHECK(m.n_embd == native.n_embd);
    CHECK(m.n_layer == native.n_layer);
    CHECK(m.n_layer_nextn == native.n_layer_nextn);
    CHECK(m.n_rot == native.n_rot);
    CHECK(m.n_ctx == native.n_ctx);
    CHECK(m.n_head == native.n_head);
    CHECK(m.n_head_kv == native.n_head_kv);
    CHECK(m.n_embd_head_k == native.n_embd_head_k);
    CHECK(m.n_embd_k_gqa == native.n_embd_k_gqa);
    CHECK(m.n_embd_head_v == native.n_embd_head_v);
    CHECK(m.n_embd_v_gqa == native.n_embd_v_gqa);
    CHECK(m.n_expert == native.n_expert);
    CHECK(m.n_expert_used == native.n_expert_used);
    CHECK(m.n_tokens == native.n_tokens);
    CHECK(m.n_outputs == native.n_outputs);
    CHECK(m.n_ctx_orig == native.n_ctx_orig);
    CHECK(m.pooling_type == (int64_t) native.pooling_type);
    CHECK(m.rope_type == (int64_t) native.rope_type);
    CHECK(m.freq_base == native.freq_base);
    CHECK(m.freq_scale == native.freq_scale);
    CHECK(m.ext_factor == native.ext_factor);
    CHECK(m.attn_factor == native.attn_factor);
    CHECK(m.beta_fast == native.beta_fast);
    CHECK(m.beta_slow == native.beta_slow);
    CHECK(m.norm_eps == native.norm_eps);
    CHECK(m.norm_rms_eps == native.norm_rms_eps);
    CHECK(m.sched == (int64_t) (intptr_t) native.sched);
    CHECK(m.backend_cpu == (int64_t) (intptr_t) native.backend_cpu);
    CHECK(m.cvec == (int64_t) (intptr_t) native.cvec);
    CHECK(m.loras == (int64_t) (intptr_t) native.loras);
    CHECK(m.mctx == (int64_t) (intptr_t) native.mctx);
    CHECK(m.cross == (int64_t) (intptr_t) native.cross);
    CHECK(m.moe_cache == (int64_t) (intptr_t) native.moe_cache);
    CHECK(m.prec_policy == (int64_t) (intptr_t) native.prec_policy);
    CHECK(m.hdmd == (int64_t) (intptr_t) native.hdmd);
    CHECK(m.res == (int64_t) (intptr_t) native.res);

    auto * t = ggml_new_tensor_1d(res.get_ctx(), GGML_TYPE_F32, 4);
    CHECK(fg153_goose_ctx(3, (int64_t) (intptr_t) &p, 0, 0, 0, 0) == 1);
    CHECK((native.crop_before_nextn(t) ? 1 : 0) ==
          fg153_goose_ctx(1, (int64_t) (intptr_t) &p, 0, (int64_t) (intptr_t) t, 0, 0));
    CHECK((native.crop_after_nextn(t) ? 1 : 0) ==
          fg153_goose_ctx(2, (int64_t) (intptr_t) &p, 0, (int64_t) (intptr_t) t, 0, 0));

    llm_graph_params q = p;
    q.cparams.embeddings_nextn = true;
    q.cparams.embeddings_nextn_masked = true;
    CHECK((native.crop_before_nextn(t) ? 1 : 0) ==
          fg153_goose_ctx(1, (int64_t) (intptr_t) &q, 0, (int64_t) (intptr_t) t, 0, 0));
    CHECK((native.crop_after_nextn(t) ? 1 : 0) ==
          fg153_goose_ctx(2, (int64_t) (intptr_t) &q, 0, (int64_t) (intptr_t) t, 0, 0));

    q.cparams.embeddings_nextn = true;
    q.cparams.embeddings_nextn_masked = false;
    CHECK((native.crop_before_nextn(t) ? 1 : 0) ==
          fg153_goose_ctx(1, (int64_t) (intptr_t) &q, 0, (int64_t) (intptr_t) t, 0, 0));
    CHECK((native.crop_after_nextn(t) ? 1 : 0) ==
          fg153_goose_ctx(2, (int64_t) (intptr_t) &q, 0, (int64_t) (intptr_t) t, 0, 0));

    llm_graph_params nullres = p;
    nullres.res = nullptr;
    CHECK(fg153_goose_ctx(3, (int64_t) (intptr_t) &nullres, 0, 0, 0, 0) == 0);

    std::puts("PASS llm_graph_context resource metadata native/Goose equality");
}

int main() {
    std::puts("worker-7 graph-result/input native-vs-Goose corpus");
    result_lifecycle();
    input_corpus();
    context_metadata();
    std::puts("PASS all graph-result/input differential and failure cases");
    return 0;
}
