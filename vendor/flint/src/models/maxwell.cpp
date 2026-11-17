#include "models.h"
#include "llama-kv-cache-msa.h"
#include <vector>

// Maxwell hyperparameters kept out of llama_hparams so an existing object layout
// (created by accepted objects) is never shifted.
static struct { uint32_t block_size, support_blocks, prefix_window, remote_blocks; } MW_CFG;

// Per-graph MSA state: position/cell maps and the block selection inputs. Same
// role as minimax-m3's input class, with the Maxwell route bias applied on the
// host side (prefix and local blocks are forced, remote blocks are ranked).
class mw_input_msa : public llm_graph_input_i {
public:
    mw_input_msa(const llama_kv_cache_msa_context * mctx, int blk, int local, int prefix_blocks)
        : mctx(mctx), blk(blk), local(local), prefix_blocks(prefix_blocks) {}

    void set_input(const llama_ubatch * ubatch) override {
        if (pos_slot_i) { mctx->set_input_pos_slot(pos_slot_i, ubatch); }
        if (pos_slot_f) { mctx->set_input_pos_slot(pos_slot_f, ubatch); }
        if (pos_mask)   { mctx->set_input_pos_mask(pos_mask, ubatch); }
        if (cell_blk && ubatch->pos) {
            mctx->set_input_cell_pos(cell_blk, ubatch, blk);
        }
        // Route bias, the same decision as the Goose mx_route_bias: prefix
        // blocks up to the query and the local window are forced.
        if (bias && ubatch->pos) {
            const int64_t n_tok = ubatch->n_tokens;
            const int64_t nb    = bias->ne[0];
            std::vector<float> data((size_t) nb * n_tok, 0.0f);
            for (int64_t i = 0; i < n_tok; ++i) {
                const int64_t cur = ubatch->pos[i] / blk;
                for (int64_t b = 0; b < prefix_blocks && b <= cur && b < nb; ++b) {
                    data[(size_t) i * nb + b] = 1e30f;
                }
                for (int l = 0; l < local; ++l) {
                    const int64_t b = cur - l;
                    if (b >= prefix_blocks && b >= 0 && b < nb) {
                        data[(size_t) i * nb + b] = 1e30f;
                    }
                }
            }
            ggml_backend_tensor_set(bias, data.data(), 0, data.size() * sizeof(float));
        }
    }

    bool can_reuse(const llm_graph_params & params) override {
        const auto * m2 = static_cast<const llama_kv_cache_msa_context *>(params.mctx);
        mctx = m2;
        const int64_t n_ps = GGML_PAD((int64_t) m2->get_n_pos(), blk);
        const int64_t ns = params.cparams.kv_unified ? 1 : params.ubatch.n_seqs_unq;
        const bool decode = params.ubatch.n_tokens == ns;
        bool res = true;
        res &= pos_mask->ne[0] == n_ps;
        res &= pos_mask->ne[1] == params.ubatch.n_tokens;
        res &= pos_slot_i->ne[0] == n_ps;
        res &= pos_slot_i->ne[1] == ns;
        res &= decode == (pos_slot_f != nullptr);
        res &= decode == (cell_blk == nullptr);
        if (pos_slot_f) { res &= pos_slot_f->ne[0] == n_ps && pos_slot_f->ne[1] == ns; }
        if (cell_blk)   { res &= cell_blk->ne[0] == (int64_t) m2->get_base()->get_n_kv() && cell_blk->ne[1] == ns; }
        return res;
    }

    ggml_tensor * pos_mask   = nullptr; // F32 [n_ps, n_tokens] 0/-inf visibility
    ggml_tensor * pos_slot_i = nullptr; // I32 [n_ps, ns]       pos -> cell
    ggml_tensor * pos_slot_f = nullptr; // F32 [n_ps, ns]       decode gather
    ggml_tensor * cell_blk   = nullptr; // I32 [n_kv, ns]       cell -> position block
    ggml_tensor * bias       = nullptr; // F32 [nblk, n_tokens] forced route blocks

    const llama_kv_cache_msa_context * mctx;
    int blk;
    int local;
    int prefix_blocks;
};

// Maxwell-V1-Flash (maxwell_moe).
//
// Reference: huggingface.co/maxwell-model/Maxwell-V1-Flash, modeling_maxwell_moe.py
// (revision ca536c43). Trunk: 45 layers, GQA 32/4 heads x 128, RMSNorm, QK-norm,
// RoPE theta 2e7, mHC residual streams (4) with Sinkhorn, sigmoid router with an
// expert bias, shared expert, dense FFN on the leading three layers.
//
// Block-sparse GQA is exact dense causal attention while the KV length is within
// the total support (prefix 1024 + local 1024 + 16 remote blocks of 128 = 4096
// tokens): the union of prefix, remote and local blocks covers every block up to
// the query position. This builder therefore uses the ordinary causal attention
// and leaves the indexer to the >4096 path, which is not built yet.
//
// The mHC decisions (which stream gates, the Sinkhorn projection, the learned
// head contraction) are the same ones proved in
// experiments/EXP-2026-11-17-926-maxwell-v1-flash-goose; the graph below applies
// them with ggml ops, as glm5-next/deepseek4 do for its own mHC.

static size_t mw_elem_offset(const ggml_tensor * t, int64_t i) {
    return ggml_row_size(t->type, i);
}

static ggml_tensor * mw_view_1d(ggml_context * ctx, ggml_tensor * t, int64_t ne0, size_t off) {
    return ggml_view_1d(ctx, t, ne0, off);
}

static ggml_tensor * mw_view_2d(ggml_context * ctx, ggml_tensor * t, int64_t ne0, int64_t ne1,
                               size_t nb1, size_t off) {
    return ggml_view_2d(ctx, t, ne0, ne1, nb1, off);
}

// h = rms_norm(x) * fn + bias, then split into the three mHC gates.
// Maxwell computes proj = W x and multiplies by r = 1/(||x||/sqrt(nC) + eps);
// rms_norm folds the same scale into the projection (eps sits under the sqrt
// instead of outside it, a ~1e-9 relative difference at hidden 2048).
static ggml_tensor * mw_hc_gates(
        ggml_context * ctx,
        llm_graph_result * res,
        int           il,
        bool          fused_pre,
        bool          fused_comb,
        ggml_tensor  * fn,
        ggml_tensor  * scale,
        ggml_tensor  * base,
        ggml_tensor  * x,
        int64_t        hc,
        int64_t        iters,
        float          eps,
        ggml_tensor ** post,
        ggml_tensor ** comb) {
    const int64_t n_embd = x->ne[0];
    const int64_t nt     = x->ne[2];

    ggml_tensor * flat  = ggml_reshape_2d(ctx, x, hc*n_embd, nt);
    ggml_tensor * nflat = ggml_rms_norm(ctx, flat, eps);
    ggml_tensor * mixes = ggml_mul_mat(ctx, fn, nflat); // [hc_mix_dim, nt]

    ggml_tensor * scale_pre  = mw_view_1d(ctx, scale, 1, mw_elem_offset(scale, 0));
    ggml_tensor * scale_post = mw_view_1d(ctx, scale, 1, mw_elem_offset(scale, 1));
    ggml_tensor * scale_comb = mw_view_1d(ctx, scale, 1, mw_elem_offset(scale, 2));

    ggml_tensor * base_pre  = mw_view_1d(ctx, base, hc, mw_elem_offset(base, 0));
    ggml_tensor * base_post = mw_view_1d(ctx, base, hc, mw_elem_offset(base, hc));
    ggml_tensor * base_comb = mw_view_1d(ctx, base, hc*hc, mw_elem_offset(base, 2*hc));

    ggml_tensor * pre = mw_view_2d(ctx, mixes, hc, nt, mixes->nb[1], 0);
    pre = ggml_mul(ctx, pre, scale_pre);
    pre = ggml_add(ctx, pre, base_pre);
    pre = ggml_add(ctx, ggml_sigmoid(ctx, pre), ggml_fill(ctx, ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 1), eps));

    *post = mw_view_2d(ctx, mixes, hc, nt, mixes->nb[1], mw_elem_offset(mixes, hc));
    *post = ggml_mul(ctx, *post, scale_post);
    *post = ggml_add(ctx, *post, base_post);
    *post = ggml_scale(ctx, ggml_sigmoid(ctx, *post), 2.0f);

    if (fused_comb) {
        *comb = ggml_dsv4_hc_comb(ctx, mixes, scale, base, eps, (int32_t) iters);
        res->add_fused_node({LLM_FUSED_OP_DSV4_HC_COMB, *comb, il});
    } else {
        ggml_tensor * c = mw_view_2d(ctx, mixes, hc*hc, nt, mixes->nb[1], mw_elem_offset(mixes, 2*hc));
        c = ggml_mul(ctx, c, scale_comb);
        c = ggml_add(ctx, c, base_comb);
        c = ggml_reshape_3d(ctx, c, hc, hc, nt); // [dst, src, nt]

        // Sinkhorn: row (dst) softmax + eps, one column (src) norm, then
        // sinkhorn_iters-1 x (row norm, column norm).
        c = ggml_soft_max(ctx, c);
        ggml_tensor * eps_t = ggml_fill(ctx, ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 1), eps);
        c = ggml_add(ctx, c, eps_t);
        for (int64_t it = 0; it < iters; ++it) {
            if (it > 0) {
                ggml_tensor * rs = ggml_sum_rows(ctx, c);
                c = ggml_div(ctx, c, ggml_add(ctx, rs, eps_t));
            }
            ggml_tensor * cs = ggml_sum_rows(ctx, ggml_cont(ctx, ggml_permute(ctx, c, 1, 0, 2, 3)));
            cs = ggml_permute(ctx, ggml_add(ctx, cs, eps_t), 1, 0, 2, 3);
            c = ggml_div(ctx, c, cs);
        }
        *comb = c;
    }

    if (fused_pre) {
        ggml_tensor * out = ggml_dsv4_hc_pre(ctx, x, pre);
        res->add_fused_node({LLM_FUSED_OP_DSV4_HC_PRE, out, il});
        return out;
    }

    // aggregate: sum_h pre_h * x_h
    ggml_tensor * out = nullptr;
    for (int64_t ih = 0; ih < hc; ++ih) {
        ggml_tensor * xh  = mw_view_2d(ctx, x, n_embd, nt, x->nb[2], ih*x->nb[1]);
        ggml_tensor * ph  = mw_view_2d(ctx, pre, 1, nt, pre->nb[1], ih*pre->nb[0]);
        ggml_tensor * cur = ggml_mul(ctx, xh, ph);
        out = out ? ggml_add(ctx, out, cur) : cur;
    }
    return out;
}

// expand_and_mix: out_dst = post_dst * layer_out + sum_src comb[dst, src] * residual_src
static ggml_tensor * mw_hc_post(
        ggml_context * ctx,
        llm_graph_result * res,
        int           il,
        bool          fused,
        ggml_tensor  * layer_out,
        ggml_tensor  * residual,
        ggml_tensor  * post,
        ggml_tensor  * comb,
        int64_t        hc) {
    const int64_t n_embd = layer_out->ne[0];
    const int64_t nt     = layer_out->ne[1];

    if (fused) {
        ggml_tensor * out = ggml_dsv4_hc_post(ctx, layer_out, residual, post, comb);
        res->add_fused_node({LLM_FUSED_OP_DSV4_HC_POST, out, il});
        return out;
    }

    ggml_tensor * out = nullptr;
    for (int64_t dst = 0; dst < hc; ++dst) {
        ggml_tensor * pd  = mw_view_2d(ctx, post, 1, nt, post->nb[1], dst*post->nb[0]);
        ggml_tensor * cur = ggml_mul(ctx, layer_out, pd);
        for (int64_t src = 0; src < hc; ++src) {
            ggml_tensor * rs = mw_view_2d(ctx, residual, n_embd, nt, residual->nb[2], src*residual->nb[1]);
            ggml_tensor * cd = mw_view_2d(ctx, comb, 1, nt, comb->nb[2], dst*comb->nb[0] + src*comb->nb[1]);
            cur = ggml_add(ctx, cur, ggml_mul(ctx, rs, cd));
        }
        cur = ggml_reshape_3d(ctx, cur, n_embd, 1, nt);
        out = out ? ggml_concat(ctx, out, cur, 1) : cur;
    }
    return out;
}

// Learned HC head: weights = sigmoid((hc_head_fn x) * rsqrt(mean(x^2)+eps) * scale + base) + eps,
// out = sum_h weights_h * x_h.
static ggml_tensor * mw_hc_head(
        ggml_context * ctx,
        ggml_tensor  * x,
        ggml_tensor  * fn,
        ggml_tensor  * scale,
        ggml_tensor  * base,
        int64_t        hc,
        float          eps) {
    const int64_t n_embd = x->ne[0];
    const int64_t nt     = x->ne[2];

    ggml_tensor * flat = ggml_reshape_2d(ctx, x, hc*n_embd, nt);
    ggml_tensor * sq   = ggml_mul(ctx, flat, flat);
    ggml_tensor * mean = ggml_scale(ctx, ggml_sum_rows(ctx, sq), 1.0f/(float)(hc*n_embd));
    ggml_tensor * ones = ggml_fill(ctx, ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 1, nt), 1.0f);
    ggml_tensor * rsq  = ggml_div(ctx, ones,
                                  ggml_sqrt(ctx, ggml_add(ctx, mean,
                                      ggml_fill(ctx, ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 1), eps))));
    ggml_tensor * proj = ggml_mul_mat(ctx, fn, flat); // [hc, nt]
    ggml_tensor * mixes = ggml_mul(ctx, proj, rsq);
    ggml_tensor * w = ggml_add(ctx, ggml_mul(ctx, mixes, scale), base);
    w = ggml_add(ctx, ggml_sigmoid(ctx, w), ggml_fill(ctx, ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 1), eps));

    ggml_tensor * out = nullptr;
    for (int64_t ih = 0; ih < hc; ++ih) {
        ggml_tensor * xh = mw_view_2d(ctx, x, n_embd, nt, x->nb[2], ih*x->nb[1]);
        ggml_tensor * wh = mw_view_2d(ctx, w, 1, nt, w->nb[1], ih*w->nb[0]);
        ggml_tensor * cur = ggml_mul(ctx, xh, wh);
        out = out ? ggml_add(ctx, out, cur) : cur;
    }
    return out;
}

void llama_model_maxwell::load_arch_hparams(llama_model_loader & ml) {
    ml.get_key(LLM_KV_ATTENTION_LAYERNORM_RMS_EPS,       hparams.f_norm_rms_eps);
    ml.get_key_or_arr(LLM_KV_EXPERT_FEED_FORWARD_LENGTH, hparams.n_ff_exp_arr, hparams.n_layer_all, false);
    ml.get_key(LLM_KV_EXPERT_SHARED_FEED_FORWARD_LENGTH, hparams.n_ff_shexp, false);
    ml.get_key(LLM_KV_LEADING_DENSE_BLOCK_COUNT,         hparams.n_layer_dense_lead, false);
    ml.get_key(LLM_KV_EXPERT_WEIGHTS_NORM,               hparams.expert_weights_norm, false);
    ml.get_key(LLM_KV_EXPERT_WEIGHTS_SCALE,              hparams.expert_weights_scale, false);
    ml.get_key(LLM_KV_EXPERT_GATING_FUNC,                hparams.expert_gating_func, false);
    if (hparams.expert_gating_func == LLAMA_EXPERT_GATING_FUNC_TYPE_NONE) {
        hparams.expert_gating_func = LLAMA_EXPERT_GATING_FUNC_TYPE_SIGMOID;
    }
    hparams.expert_weights_norm = true;

    ml.get_key(LLM_KV_MAXWELL_BSGQA_BLOCK_SIZE,               MW_CFG.block_size);
    ml.get_key(LLM_KV_MAXWELL_BSGQA_SUPPORT_BLOCKS,           MW_CFG.support_blocks);
    ml.get_key(LLM_KV_MAXWELL_BSGQA_PREFIX_WINDOW,            MW_CFG.prefix_window, false);
    ml.get_key(LLM_KV_MAXWELL_BSGQA_REMOTE_BLOCKS,            MW_CFG.remote_blocks, false);
    ml.get_key(LLM_KV_ATTENTION_INDEXER_HEAD_COUNT,           hparams.indexer_n_head,    false);
    ml.get_key(LLM_KV_ATTENTION_INDEXER_KEY_LENGTH,           hparams.indexer_head_size, false);
    ml.get_key(LLM_KV_ATTENTION_INDEXER_TOP_K,                hparams.indexer_top_k,     false);
    ml.get_key(LLM_KV_ATTENTION_INDEXER_BLOCK_SIZE,           hparams.indexer_block_size, false);
    ml.get_key(LLM_KV_ATTENTION_INDEXER_LOCAL_BLOCKS,         hparams.indexer_local_blocks, false);

    ml.get_key(LLM_KV_HYPER_CONNECTION_COUNT,               hparams.dsv4_hc_mult);
    ml.get_key(LLM_KV_HYPER_CONNECTION_SINKHORN_ITERATIONS, hparams.dsv4_hc_sinkhorn_iters);
    ml.get_key(LLM_KV_HYPER_CONNECTION_EPSILON,             hparams.dsv4_hc_eps);
    GGML_ASSERT(hparams.dsv4_hc_mult == 4 && "Maxwell mHC requires four residual streams");
    GGML_ASSERT(hparams.dsv4_hc_sinkhorn_iters > 0);
    GGML_ASSERT(hparams.dsv4_hc_eps > 0.0f);

    switch (hparams.n_layer()) {
        case 45: type = LLM_TYPE_36B; break;
        default: type = LLM_TYPE_UNKNOWN;
    }
}

void llama_model_maxwell::load_arch_tensors(llama_model_loader & ml) {
    LLAMA_LOAD_LOCALS;

    const int64_t hc         = hparams.dsv4_hc_mult;
    const int64_t hc_mix_dim = (2 + hc)*hc;

    tok_embd    = create_tensor(tn(LLM_TENSOR_TOKEN_EMBD,  "weight"), {n_embd, n_vocab}, 0);
    output_norm = create_tensor(tn(LLM_TENSOR_OUTPUT_NORM, "weight"), {n_embd}, 0);
    output      = create_tensor(tn(LLM_TENSOR_OUTPUT,      "weight"), {n_embd, n_vocab}, 0);

    hc_head_fn    = create_tensor(tn(LLM_TENSOR_HC_HEAD_FN,    "weight"), {hc*n_embd, hc}, 0);
    hc_head_base  = create_tensor(tn(LLM_TENSOR_HC_HEAD_BASE,  "weight"), {hc}, 0);
    hc_head_scale = create_tensor(tn(LLM_TENSOR_HC_HEAD_SCALE, "weight"), {1}, 0);

    for (int i = 0; i < n_layer; ++i) {
        auto & layer = layers[i];

        layer.attn_norm = create_tensor(tn(LLM_TENSOR_ATTN_NORM, "weight", i), {n_embd}, 0);
        layer.ffn_norm  = create_tensor(tn(LLM_TENSOR_FFN_NORM,  "weight", i), {n_embd}, 0);

        layer.wq = create_tensor(tn(LLM_TENSOR_ATTN_Q, "weight", i), {n_embd, n_embd_head_k*n_head}, 0);
        layer.wk = create_tensor(tn(LLM_TENSOR_ATTN_K, "weight", i), {n_embd, n_embd_head_k*n_head_kv}, 0);
        layer.wv = create_tensor(tn(LLM_TENSOR_ATTN_V, "weight", i), {n_embd, n_embd_head_v*n_head_kv}, 0);
        layer.wo = create_tensor(tn(LLM_TENSOR_ATTN_OUT, "weight", i), {n_embd_head_v*n_head, n_embd}, 0);

        layer.attn_q_norm = create_tensor(tn(LLM_TENSOR_ATTN_Q_NORM, "weight", i), {n_embd_head_k}, 0);
        layer.attn_k_norm = create_tensor(tn(LLM_TENSOR_ATTN_K_NORM, "weight", i), {n_embd_head_k}, 0);

        // Block-sparse indexer. Optional so a GGUF without it still loads for the
        // dense <=support path; the builder refuses long contexts in that case.
        if (hparams.indexer_head_size > 0) {
            layer.index_q_proj = create_tensor(tn(LLM_TENSOR_INDEXER_Q_PROJ, "weight", i),
                    {n_embd, (int64_t) hparams.indexer_n_head * hparams.indexer_head_size}, TENSOR_NOT_REQUIRED);
            layer.index_k_proj = create_tensor(tn(LLM_TENSOR_INDEXER_K_PROJ, "weight", i),
                    {n_embd, hparams.indexer_head_size}, TENSOR_NOT_REQUIRED);
            layer.index_q_norm = create_tensor(tn(LLM_TENSOR_INDEXER_Q_NORM, "weight", i),
                    {hparams.indexer_head_size}, TENSOR_NOT_REQUIRED);
            layer.index_k_norm = create_tensor(tn(LLM_TENSOR_INDEXER_K_NORM, "weight", i),
                    {hparams.indexer_head_size}, TENSOR_NOT_REQUIRED);
        }

        layer.hc_attn_fn    = create_tensor(tn(LLM_TENSOR_HC_ATTN_FN,    "weight", i), {hc*n_embd, hc_mix_dim}, 0);
        layer.hc_attn_base  = create_tensor(tn(LLM_TENSOR_HC_ATTN_BASE,  "weight", i), {hc_mix_dim}, 0);
        layer.hc_attn_scale = create_tensor(tn(LLM_TENSOR_HC_ATTN_SCALE, "weight", i), {3}, 0);
        layer.hc_ffn_fn     = create_tensor(tn(LLM_TENSOR_HC_FFN_FN,     "weight", i), {hc*n_embd, hc_mix_dim}, 0);
        layer.hc_ffn_base   = create_tensor(tn(LLM_TENSOR_HC_FFN_BASE,   "weight", i), {hc_mix_dim}, 0);
        layer.hc_ffn_scale  = create_tensor(tn(LLM_TENSOR_HC_FFN_SCALE,  "weight", i), {3}, 0);

        if ((uint32_t) i < hparams.n_layer_dense_lead) {
            layer.ffn_gate = create_tensor(tn(LLM_TENSOR_FFN_GATE, "weight", i), {n_embd, n_ff}, 0);
            layer.ffn_up   = create_tensor(tn(LLM_TENSOR_FFN_UP,   "weight", i), {n_embd, n_ff}, 0);
            layer.ffn_down = create_tensor(tn(LLM_TENSOR_FFN_DOWN, "weight", i), {n_ff, n_embd}, 0);
        } else {
            const int64_t n_ff_exp = hparams.n_ff_exp();

            layer.ffn_gate_inp  = create_tensor(tn(LLM_TENSOR_FFN_GATE_INP,  "weight", i), {n_embd, n_expert}, 0);
            layer.ffn_exp_probs_b = create_tensor(tn(LLM_TENSOR_FFN_EXP_PROBS_B, "weight", i), {n_expert}, 0);
            layer.ffn_gate_exps = create_tensor(tn(LLM_TENSOR_FFN_GATE_EXPS, "weight", i), {n_embd, n_ff_exp, n_expert}, 0);
            layer.ffn_up_exps   = create_tensor(tn(LLM_TENSOR_FFN_UP_EXPS,   "weight", i), {n_embd, n_ff_exp, n_expert}, 0);
            layer.ffn_down_exps = create_tensor(tn(LLM_TENSOR_FFN_DOWN_EXPS, "weight", i), {n_ff_exp, n_embd, n_expert}, 0);

            const int64_t n_ff_shexp = hparams.n_ff_shexp ? hparams.n_ff_shexp : n_ff;
            layer.ffn_gate_shexp = create_tensor(tn(LLM_TENSOR_FFN_GATE_SHEXP, "weight", i), {n_embd, n_ff_shexp}, 0);
            layer.ffn_up_shexp   = create_tensor(tn(LLM_TENSOR_FFN_UP_SHEXP,   "weight", i), {n_embd, n_ff_shexp}, 0);
            layer.ffn_down_shexp = create_tensor(tn(LLM_TENSOR_FFN_DOWN_SHEXP, "weight", i), {n_ff_shexp, n_embd}, 0);
        }
    }
}

std::unique_ptr<llm_graph_context> llama_model_maxwell::build_arch_graph(const llm_graph_params & params) const {
    return std::make_unique<graph>(*this, params);
}

llama_model_maxwell::graph::graph(const llama_model & model, const llm_graph_params & params) :
    llm_graph_context(params), model(model) {
    const int64_t n_embd_head = hparams.n_embd_head_v();
    const int64_t hc          = hparams.dsv4_hc_mult;
    const float   eps         = hparams.dsv4_hc_eps;

    // Above the support window the block-sparse route drops blocks; this builder
    // does not implement the indexer yet, so refuse instead of returning a
    // silently different attention.
    const int64_t support = (int64_t) MW_CFG.block_size *
                            (int64_t) MW_CFG.support_blocks;
    if (support > 0 && (int64_t) cparams.n_ctx > support) { // lifted when the sparse branch lands
        throw std::runtime_error(
            "maxwell_moe: the block-sparse indexer is not implemented; "
            "context above " + std::to_string(support) + " tokens is refused");
    }

    GGML_ASSERT(n_embd_head == hparams.n_embd_head_k());

    ggml_tensor * cur;
    ggml_tensor * inpL;

    inpL = build_inp_embd(model.tok_embd);
    cb(inpL, "model.input_embed", -1);

    ggml_tensor * inp_pos = build_inp_pos();

    const bool has_indexer = model.layers[0].index_q_proj != nullptr;
    const bool msa_enabled = false; // B2 will enable the sparse branch; dense until then

    auto * inp_attn = build_attn_inp_kv_msa(msa_enabled);

    mw_input_msa * msa = nullptr;
    ggml_tensor * msa_mf = nullptr;
    int64_t n_kv = 0, n_ps = 0, nblk = 0, ns = 1, n_tps = 0;
    bool msa_decode = false;

    // MSA input tensors are created together with the sparse branch (B2);
    // creating them unused makes the scheduler assert on a buffer-less input.
    GGML_UNUSED(msa);
    GGML_UNUSED(msa_mf);
    GGML_UNUSED(n_kv); GGML_UNUSED(n_ps); GGML_UNUSED(nblk); GGML_UNUSED(ns);
    GGML_UNUSED(n_tps); GGML_UNUSED(msa_decode);

    ggml_tensor * inp_out_ids = build_inp_out_ids();

    // expand the single stream into hc residual streams
    ggml_tensor * x = ggml_reshape_3d(ctx0, inpL, n_embd, 1, n_tokens);
    x = ggml_repeat_4d(ctx0, x, n_embd, hc, n_tokens, 1);
    cb(x, "hc_init", -1);

    for (int il = 0; il < n_layer; ++il) {
        const auto & layer = model.layers[il];
        res->t_layer_inp[il] = x;

        // --- attention with mHC ---
        ggml_tensor * residual = x;
        ggml_tensor * post = nullptr;
        ggml_tensor * comb = nullptr;

        cur = mw_hc_gates(ctx0, res, il, cparams.fused_dsv4_hc_pre, cparams.fused_dsv4_hc_comb,
                          layer.hc_attn_fn, layer.hc_attn_scale, layer.hc_attn_base,
                          x, hc, (int64_t) hparams.dsv4_hc_sinkhorn_iters, eps, &post, &comb);
        cb(cur, "hc_attn_pre", il);

        cur = build_norm(cur, layer.attn_norm, nullptr, LLM_NORM_RMS, il);
        cb(cur, "attn_norm", il);

        auto [Qcur, Kcur, Vcur] = build_qkv(layer, cur, n_embd_head, n_head, n_head_kv, il);
        cb(Qcur, "Qcur", il);
        cb(Kcur, "Kcur", il);
        cb(Vcur, "Vcur", il);

        Qcur = build_norm(Qcur, layer.attn_q_norm, nullptr, LLM_NORM_RMS, il);
        Kcur = build_norm(Kcur, layer.attn_k_norm, nullptr, LLM_NORM_RMS, il);
        cb(Qcur, "Qcur_normed", il);
        cb(Kcur, "Kcur_normed", il);

        Qcur = ggml_rope_ext(ctx0, Qcur, inp_pos, nullptr, n_rot, rope_type, n_ctx_orig,
                             freq_base, freq_scale, ext_factor, attn_factor, beta_fast, beta_slow);
        Kcur = ggml_rope_ext(ctx0, Kcur, inp_pos, nullptr, n_rot, rope_type, n_ctx_orig,
                             freq_base, freq_scale, ext_factor, attn_factor, beta_fast, beta_slow);
        cb(Qcur, "Qcur_rope", il);
        cb(Kcur, "Kcur_rope", il);

        cur = build_attn(inp_attn, layer.wo, layer.wo_b, layer.wo_s,
                         Qcur, Kcur, Vcur, nullptr, nullptr, nullptr,
                         1.0f/sqrtf(float(n_embd_head)), il);
        cb(cur, "attn_out", il);

        x = mw_hc_post(ctx0, res, il, cparams.fused_dsv4_hc_post, cur, residual, post, comb, hc);
        cb(x, "hc_attn_post", il);

        // --- feed-forward with mHC ---
        residual = x;
        cur = mw_hc_gates(ctx0, res, il, cparams.fused_dsv4_hc_pre, cparams.fused_dsv4_hc_comb,
                          layer.hc_ffn_fn, layer.hc_ffn_scale, layer.hc_ffn_base,
                          x, hc, (int64_t) hparams.dsv4_hc_sinkhorn_iters, eps, &post, &comb);
        cb(cur, "hc_ffn_pre", il);

        cur = build_norm(cur, layer.ffn_norm, nullptr, LLM_NORM_RMS, il);
        cb(cur, "ffn_norm", il);

        if ((uint32_t) il < hparams.n_layer_dense_lead) {
            cur = build_ffn(cur,
                    layer.ffn_up,   nullptr, nullptr,
                    layer.ffn_gate, nullptr, nullptr,
                    layer.ffn_down, nullptr, nullptr,
                    nullptr, LLM_FFN_SILU, LLM_FFN_PAR, il);
            cb(cur, "ffn_out", il);
        } else {
            ggml_tensor * moe = build_moe_ffn(cur,
                    layer.ffn_gate_inp,
                    layer.ffn_up_exps,
                    layer.ffn_gate_exps,
                    layer.ffn_down_exps,
                    layer.ffn_exp_probs_b,
                    n_expert, n_expert_used,
                    LLM_FFN_SILU, true, 1.0f,
                    LLAMA_EXPERT_GATING_FUNC_TYPE_SIGMOID, il);
            cb(moe, "ffn_moe_out", il);

            ggml_tensor * shexp = build_ffn(cur,
                    layer.ffn_up_shexp,   nullptr, nullptr,
                    layer.ffn_gate_shexp, nullptr, nullptr,
                    layer.ffn_down_shexp, nullptr, nullptr,
                    nullptr, LLM_FFN_SILU, LLM_FFN_PAR, il);
            cb(shexp, "ffn_shexp", il);

            cur = ggml_add(ctx0, moe, shexp);
            cb(cur, "ffn_out", il);
        }

        x = mw_hc_post(ctx0, res, il, cparams.fused_dsv4_hc_post, cur, residual, post, comb, hc);
        x = build_cvec(x, il);
        cb(x, "l_out", il);
    }

    // collapse the streams with the learned head, then the final norm and LM head
    if (inp_out_ids) {
        ggml_tensor * flat = ggml_reshape_2d(ctx0, x, n_embd*hc, n_tokens);
        flat = ggml_get_rows(ctx0, flat, inp_out_ids);
        x = ggml_reshape_3d(ctx0, flat, n_embd, hc, n_outputs);
    }

    cur = mw_hc_head(ctx0, x, model.hc_head_fn, model.hc_head_scale, model.hc_head_base, hc, eps);
    cb(cur, "hc_head", -1);

    cur = build_norm(cur, model.output_norm, nullptr, LLM_NORM_RMS, -1);
    cb(cur, "result_norm", -1);
    res->t_embd = cur;

    cur = build_lora_mm(model.output, cur, model.output_s);
    cb(cur, "result_output", -1);
    res->t_logits = cur;
}
