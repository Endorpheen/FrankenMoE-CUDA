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

// Flash attention over gathered sparse keys: [D,HQ,T] x [D,n_keys,1,C] with a
// gathered F16 mask. Same layout contract as minimax-m3's MSA FA helper.
static ggml_tensor * mw_attn_msa_fa(ggml_context * ctx, ggml_tensor * q_cur,
        ggml_tensor * k, ggml_tensor * v, ggml_tensor * mask, int64_t Gp, float kq_scale) {
    const int64_t D  = q_cur->ne[0];
    const int64_t HQ = q_cur->ne[1];
    const int64_t T  = q_cur->ne[2];
    const int64_t C  = k->ne[3];
    const int64_t R  = HQ*T/(Gp*C);
    GGML_ASSERT(Gp*C*R == HQ*T);
    ggml_tensor * q = ggml_reshape_4d(ctx, q_cur, D, Gp, C, R);
    q = ggml_permute(ctx, q, 0, 2, 3, 1);
    ggml_tensor * o = ggml_flash_attn_ext(ctx, q, k, v, mask, kq_scale, 0.0f, 0.0f);
    ggml_prec_set_acc(o, GGML_PREC_F32);
    o = ggml_permute(ctx, o, 0, 1, 3, 2);
    if (!ggml_is_contiguous(o)) {
        o = ggml_cont(ctx, o);
    }
    return ggml_reshape_2d(ctx, o, D*HQ, T);
}


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
    if (support > 0 && (int64_t) cparams.n_ctx > support && model.layers[0].index_q_proj == nullptr) {
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
    const bool msa_enabled = has_indexer && support > 0 && (int64_t) cparams.n_ctx > support;

    auto * inp_attn = build_attn_inp_kv_msa(msa_enabled);

    mw_input_msa * msa = nullptr;
    ggml_tensor * msa_mf = nullptr;
    int64_t n_kv = 0, n_ps = 0, nblk = 0, ns = 1, n_tps = 0;
    bool msa_decode = false;

    if (msa_enabled) {
        const auto * mctx_msa = static_cast<const llama_kv_cache_msa_context *>(mctx);
        ggml_tensor * kqm = inp_attn->get_kq_mask();
        n_kv  = kqm->ne[0];
        n_tps = kqm->ne[1];
        ns    = kqm->ne[3];
        {
            const int blk = (int) MW_CFG.block_size;
            n_ps = GGML_PAD((int64_t) mctx_msa->get_n_pos(), blk);
            nblk = n_ps / blk;
            msa_decode = n_tps == 1;
            auto in = std::make_unique<mw_input_msa>(mctx_msa, blk,
                    (int) hparams.indexer_local_blocks,
                    (int) (MW_CFG.prefix_window / MW_CFG.block_size));
            in->bias = ggml_new_tensor_2d(ctx0, GGML_TYPE_F32, nblk, n_tokens);
            ggml_set_input(in->bias);
            in->pos_mask = ggml_new_tensor_2d(ctx0, GGML_TYPE_F32, n_ps, n_tokens);
            ggml_set_input(in->pos_mask);
            in->pos_slot_i = ggml_new_tensor_2d(ctx0, GGML_TYPE_I32, n_ps, ns);
            ggml_set_input(in->pos_slot_i);
            if (msa_decode) {
                in->pos_slot_f = ggml_new_tensor_2d(ctx0, GGML_TYPE_F32, n_ps, ns);
                ggml_set_input(in->pos_slot_f);
            } else {
                in->cell_blk = ggml_new_tensor_2d(ctx0, GGML_TYPE_I32, n_kv, ns);
                ggml_set_input(in->cell_blk);
                msa_mf = ggml_cast(ctx0, kqm, GGML_TYPE_F32);
            }
            msa = (mw_input_msa *) res->add_input(std::move(in));
        }
    }

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

        if (msa_enabled) {
            const auto * mctx_msa_l = static_cast<const llama_kv_cache_msa_context *>(mctx);
            const auto * mctx_cur = mctx_msa_l->get_base();
            const auto * mctx_idx = mctx_msa_l->get_idx();
            const int64_t idx_hd    = hparams.indexer_head_size;
            const int64_t idx_heads = hparams.indexer_n_head;

            ggml_tensor * iq = build_lora_mm(layer.index_q_proj, cur);
            ggml_tensor * ik = build_lora_mm(layer.index_k_proj, cur);
            iq = ggml_reshape_3d(ctx0, iq, idx_hd, idx_heads, n_tokens);
            ik = ggml_reshape_3d(ctx0, ik, idx_hd, 1, n_tokens);
            iq = build_norm(iq, layer.index_q_norm, nullptr, LLM_NORM_RMS, il);
            ik = build_norm(ik, layer.index_k_norm, nullptr, LLM_NORM_RMS, il);
            iq = ggml_rope_ext(ctx0, iq, inp_pos, nullptr, n_rot, rope_type, n_ctx_orig,
                               freq_base, freq_scale, ext_factor, attn_factor, beta_fast, beta_slow);
            ik = ggml_rope_ext(ctx0, ik, inp_pos, nullptr, n_rot, rope_type, n_ctx_orig,
                               freq_base, freq_scale, ext_factor, attn_factor, beta_fast, beta_slow);
            cb(iq, "msa_iq", il);
            cb(ik, "msa_ik", il);

            ggml_build_forward_expand(gf, mctx_idx->cpy_k(ctx0, ik, inp_attn->get_k_idxs_idx(), il));
            ggml_tensor * ik_kv = mctx_idx->get_k(ctx0, il);

            ggml_build_forward_expand(gf, Qcur);
            ggml_build_forward_expand(gf, Kcur);
            ggml_build_forward_expand(gf, Vcur);
            ggml_build_forward_expand(gf, mctx_cur->cpy_k(ctx0, Kcur, inp_attn->get_k_idxs(), il));
            ggml_build_forward_expand(gf, mctx_cur->cpy_v(ctx0, Vcur, inp_attn->get_v_idxs(), il));
            ggml_tensor * k = mctx_cur->get_k(ctx0, il);
            ggml_tensor * v = mctx_cur->get_v(ctx0, il);

            const int64_t D   = k->ne[0];
            const int64_t HKV = k->ne[1];
            const int64_t Gp  = n_head / HKV;
            const int blk_i = (int) MW_CFG.block_size;
            const int64_t n_prefix = (int64_t) (MW_CFG.prefix_window / MW_CFG.block_size);
            const int64_t n_local  = (int64_t) hparams.indexer_local_blocks;
            const int64_t n_remote = (int64_t) MW_CFG.remote_blocks;
            int64_t K = n_prefix + n_local + n_remote;
            if (K > nblk) K = nblk;
            const float kq_scale = 1.0f/sqrtf(float(n_embd_head));

            ggml_tensor * kfa = nullptr;
            ggml_tensor * vfa = nullptr;
            ggml_tensor * mfa = nullptr;
            if (msa_decode) {
                // gather the indexer keys through the pos -> cell map
                ggml_tensor * ik3 = ggml_view_3d(ctx0, ik_kv, idx_hd, n_kv, ns,
                        ik_kv->nb[2], ik_kv->nb[3], 0);
                ggml_tensor * ikp = ggml_get_rows(ctx0, ik3, msa->pos_slot_i);
                ggml_tensor * iq4 = ggml_reshape_4d(ctx0, iq, idx_hd, idx_heads, 1, ns);
                ggml_tensor * sc  = ggml_mul_mat(ctx0,
                        ggml_reshape_4d(ctx0, ikp, idx_hd, n_ps, 1, ns), iq4);
                ggml_prec_set_acc(sc, GGML_PREC_F32);
                // unmapped positions come out -inf, so they can never rank into the top-k
                sc = ggml_add_inplace(ctx0, sc,
                        ggml_reshape_4d(ctx0, msa->pos_mask, n_ps, 1, 1, ns));
                ggml_tensor * bs = ggml_pool_2d(ctx0, sc, GGML_OP_POOL_MAX, blk_i, 1, blk_i, 1, 0, 0);
                cb(bs, "msa_bs", il);

                ggml_tensor * bsf = ggml_add(ctx0, bs,
                        ggml_reshape_4d(ctx0, msa->bias, nblk, 1, 1, ns));
                ggml_tensor * idx = ggml_top_k(ctx0, bsf, K);

                // pos idx:  tj[t,k,h,s] = blk*idx[k,h,s] + t
                // cell idx: cs[t,k,h,s] = pos_slot[tj]
                // row idx:  tr[t,k,h,s] = cs*HKV + h
                ggml_tensor * a = ggml_scale(ctx0, ggml_cast(ctx0, idx, GGML_TYPE_F32), (float) blk_i);
                a = ggml_reshape_4d(ctx0, a, 1, K, idx_heads, ns);
                ggml_tensor * tj = ggml_add(ctx0,
                        ggml_repeat_4d(ctx0, a, blk_i, K, idx_heads, ns),
                        ggml_reshape_3d(ctx0, ggml_arange(ctx0, 0.0f, (float) blk_i, 1.0f), blk_i, 1, 1));
                ggml_tensor * tokj = ggml_cast(ctx0, ggml_reshape_2d(ctx0, tj, (int64_t) blk_i*K*idx_heads, ns), GGML_TYPE_I32);

                ggml_tensor * cs = ggml_get_rows(ctx0,
                        ggml_reshape_3d(ctx0, msa->pos_slot_f, 1, n_ps, ns), tokj);
                cs = ggml_reshape_4d(ctx0, cs, blk_i, K, idx_heads, ns);
                ggml_tensor * tr = ggml_add(ctx0,
                        ggml_scale(ctx0, cs, (float) HKV),
                        ggml_reshape_3d(ctx0, ggml_arange(ctx0, 0.0f, (float) HKV, 1.0f), 1, 1, idx_heads));
                ggml_tensor * tokr = ggml_cast(ctx0, ggml_reshape_2d(ctx0, tr, (int64_t) blk_i*K*idx_heads, ns), GGML_TYPE_I32);

                ggml_tensor * k3 = ggml_view_3d(ctx0, k, D, HKV*n_kv, ns, k->nb[1], k->nb[3], 0);
                ggml_tensor * v3 = ggml_view_3d(ctx0, v, D, HKV*n_kv, ns, v->nb[1], v->nb[3], 0);
                ggml_tensor * mp = ggml_reshape_3d(ctx0, msa->pos_mask, 1, n_ps, ns);

                ggml_tensor * kg = ggml_get_rows(ctx0, k3, tokr);
                ggml_tensor * vg = ggml_get_rows(ctx0, v3, tokr);
                ggml_tensor * mg = ggml_get_rows(ctx0, mp, tokj);

                const ggml_type kt = ggml_is_quantized(k->type) ? GGML_TYPE_F16 : k->type;
                const ggml_type vt = ggml_is_quantized(v->type) ? GGML_TYPE_F16 : v->type;
                ggml_tensor * kfa = ggml_reshape_4d(ctx0, kg, D, (int64_t) blk_i*K, 1, idx_heads*ns);
                ggml_tensor * vfa = ggml_reshape_4d(ctx0, vg, D, (int64_t) blk_i*K, 1, idx_heads*ns);
                if (kfa->type != kt) { kfa = ggml_cast(ctx0, kfa, kt); }
                if (vfa->type != vt) { vfa = ggml_cast(ctx0, vfa, vt); }
                ggml_tensor * mfa = ggml_cast(ctx0, ggml_reshape_4d(ctx0, mg, (int64_t) blk_i*K, 1, 1, idx_heads*ns), GGML_TYPE_F16);

                cur = mw_attn_msa_fa(ctx0, Qcur, kfa, vfa, mfa, Gp, kq_scale);
            } else {
                // batch: per-stream loop
                std::vector<ggml_tensor *> outs(ns);
                for (int64_t st = 0; st < ns; ++st) {
                    ggml_tensor * iq_s = ggml_view_3d(ctx0, iq, idx_hd, idx_heads, n_tps,
                            iq->nb[1], iq->nb[2], st*n_tps*iq->nb[2]);
                    ggml_tensor * ik_s = ggml_view_2d(ctx0, ik_kv, idx_hd, n_kv,
                            ik_kv->nb[2], st*ik_kv->nb[3]);
                    ggml_tensor * psl_s = ggml_view_1d(ctx0, msa->pos_slot_i, n_ps,
                            st*msa->pos_slot_i->nb[1]);
                    ggml_tensor * pm_s = ggml_view_3d(ctx0, msa->pos_mask, n_ps, 1, n_tps,
                            msa->pos_mask->nb[1], msa->pos_mask->nb[1], st*n_tps*msa->pos_mask->nb[1]);
                    ggml_tensor * cb_s = ggml_view_1d(ctx0, msa->cell_blk, n_kv,
                            st*msa->cell_blk->nb[1]);
                    ggml_tensor * mf_s = ggml_view_3d(ctx0, msa_mf, n_kv, n_tps, 1,
                            msa_mf->nb[1], msa_mf->nb[3], st*msa_mf->nb[3]);
                    ggml_tensor * bias_s = ggml_view_3d(ctx0, msa->bias, nblk, 1, n_tps,
                            msa->bias->nb[1], msa->bias->nb[1], st*n_tps*msa->bias->nb[1]);
                    ggml_tensor * q_s = ggml_view_3d(ctx0, Qcur, D, n_head, n_tps,
                            Qcur->nb[1], Qcur->nb[2], st*n_tps*Qcur->nb[2]);
                    ggml_tensor * k_s = ggml_view_4d(ctx0, k, D, HKV, n_kv, 1,
                            k->nb[1], k->nb[2], k->nb[3], st*k->nb[3]);
                    ggml_tensor * v_s = ggml_view_4d(ctx0, v, D, HKV, n_kv, 1,
                            v->nb[1], v->nb[2], v->nb[3], st*v->nb[3]);

                    ggml_tensor * ikp = ggml_get_rows(ctx0, ik_s, psl_s);
                    ggml_tensor * sc = ggml_mul_mat(ctx0, ikp,
                            ggml_reshape_2d(ctx0, iq_s, idx_hd, idx_heads*n_tps));
                    ggml_prec_set_acc(sc, GGML_PREC_F32);
                    sc = ggml_reshape_3d(ctx0, sc, n_ps, idx_heads, n_tps);
                    sc = ggml_add_inplace(ctx0, sc, pm_s);
                    ggml_tensor * bs = ggml_pool_2d(ctx0, sc, GGML_OP_POOL_MAX, blk_i, 1, blk_i, 1, 0, 0);
                    cb(bs, "msa_bs", il);

                    ggml_tensor * bsf = ggml_add(ctx0, bs, bias_s);
                    cb(bsf, "msa_bsf", il);
                    ggml_tensor * idx = ggml_top_k(ctx0, bsf, K);

                    ggml_tensor * ninf = ggml_cast(ctx0,
                            ggml_scale_bias(ctx0, bias_s, 0.0f, -1e30f), GGML_TYPE_F16);
                    ninf = ggml_repeat_4d(ctx0, ninf, nblk, idx_heads, n_tps, 1);
                    ggml_tensor * zero = ggml_scale(ctx0, ggml_cast(ctx0, idx, GGML_TYPE_F32), 0.0f);
                    ggml_tensor * bm = ggml_set_rows(ctx0,
                            ggml_reshape_3d(ctx0, ninf, 1, nblk, idx_heads*n_tps),
                            ggml_reshape_3d(ctx0, zero, 1, K,    idx_heads*n_tps),
                            ggml_reshape_2d(ctx0, idx,     K,    idx_heads*n_tps));
                    bm = ggml_reshape_3d(ctx0, bm, nblk, idx_heads, n_tps);
                    bm = ggml_cont(ctx0, ggml_permute(ctx0, bm, 0, 2, 1, 3));
                    cb(bm, "msa_block_mask", il);

                    ggml_tensor * bm2 = ggml_cont(ctx0, ggml_transpose(ctx0,
                            ggml_reshape_2d(ctx0, bm, nblk, n_tps*idx_heads)));
                    ggml_tensor * bmc = ggml_get_rows(ctx0, bm2, cb_s);
                    ggml_tensor * bmx = ggml_cont(ctx0, ggml_transpose(ctx0, bmc));
                    bmx = ggml_reshape_3d(ctx0, bmx, n_kv, n_tps, idx_heads);
                    ggml_tensor * mask4 = ggml_add_inplace(ctx0, bmx, mf_s);
                    mask4 = ggml_cast(ctx0,
                            ggml_reshape_4d(ctx0, mask4, n_kv, n_tps, 1, idx_heads), GGML_TYPE_F16);
                    cb(mask4, "msa_mask4", il);

                    ggml_tensor * kfa = ggml_permute(ctx0, k_s, 0, 3, 1, 2);
                    ggml_tensor * vfa = ggml_permute(ctx0, v_s, 0, 3, 1, 2);
                    outs[st] = mw_attn_msa_fa(ctx0, q_s, kfa, vfa, mask4, Gp, kq_scale);
                }
                cur = outs[0];
                for (int64_t st = 1; st < ns; ++st) {
                    cur = ggml_concat(ctx0, cur, outs[st], 1);
                }
            }
            cb(cur, "msa_out", il);
            if (layer.wo) {
                cur = build_lora_mm(layer.wo, cur, layer.wo_s);
            }
        } else {
            cur = build_attn(inp_attn, layer.wo, layer.wo_b, layer.wo_s,
                             Qcur, Kcur, Vcur, nullptr, nullptr, nullptr,
                             1.0f/sqrtf(float(n_embd_head)), il);
        }


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
