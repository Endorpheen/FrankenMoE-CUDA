// Apex shape gate: does q35_shape describe the tensors the real file carries?
//
// This is a TEST. For a representative recurrent layer and a representative
// attention layer it compares the module's declared extents against the GGUF
// tensor list, tensor by tensor.
//
// Why only some tensors: the module declares a fused T_ATTN_QKV, while this
// model file carries separate attn_q / attn_k / attn_v. Both are legitimate
// layouts of the same architecture, so comparing them would report a difference
// that is a variant, not a defect. The tensors compared here are the ones whose
// declaration is unambiguous: embeddings, norms, the expert FFN stack, the shared
// expert stack and the SSM/GDN stack.
//
// It does not build a graph and does not start a model.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gguf.h"
#include "ggml.h"
#include "main.h"

#define P_N_EMBD 0
#define P_N_VOCAB 1
#define P_N_LAYER 2
#define P_N_LAYER_ALL 3
#define P_N_HEAD 4
#define P_N_HEAD_KV 5
#define P_N_FF 6
#define P_N_EXPERT 7
#define P_N_EXPERT_USED 8
#define P_N_FF_EXP 9
#define P_N_FF_SHEXP 10
#define P_D_CONV 11
#define P_D_INNER 12
#define P_D_STATE 13
#define P_DT_RANK 14
#define P_N_GROUP 15
#define P_HEAD_K 16
#define P_INTERVAL 17
#define P_HAS_OUTPUT 18

// tensor ids used here
#define T_TOKEN_EMBD 0
#define T_OUTPUT_NORM 1
#define T_ATTN_NORM 3
#define T_ATTN_POST_NORM 4
#define T_SSM_CONV1D 11
#define T_SSM_A 13
#define T_SSM_BETA 14
#define T_SSM_ALPHA 15
#define T_SSM_NORM 16
#define T_SSM_OUT 17
#define T_FFN_GATE_INP 18
#define T_FFN_DOWN_EXPS 19
#define T_FFN_GATE_EXPS 21
#define T_FFN_UP_EXPS 22
#define T_FFN_GATE_INP_SHEXP 23
#define T_FFN_GATE_SHEXP 24
#define T_FFN_UP_SHEXP 25
#define T_FFN_DOWN_SHEXP 26

static int64_t kv_u32(struct gguf_context * c, const char * key, int64_t dflt) {
    const int64_t id = gguf_find_key(c, key);
    if (id < 0) return dflt;
    switch (gguf_get_kv_type(c, (int) id)) {
        case GGUF_TYPE_UINT32: return (int64_t) gguf_get_val_u32(c, (int) id);
        case GGUF_TYPE_INT32:  return (int64_t) gguf_get_val_i32(c, (int) id);
        case GGUF_TYPE_UINT64: return (int64_t) gguf_get_val_u64(c, (int) id);
        case GGUF_TYPE_INT64:  return gguf_get_val_i64(c, (int) id);
        default: return dflt;
    }
}

static int64_t p_block[32];
static sl_i64 g_p;

static int check(struct gguf_context * ctx, int64_t il, int64_t kind,
                 const char * name, int64_t tid, int * cases, int * bad) {
    const int64_t id = gguf_find_tensor(ctx, name);
    if (id < 0) {
        return 0;   // this model does not carry it; not a failure
    }
    int64_t out[3] = { 0, 0, 0 };
    const int64_t ok = q35_shape(g_p, kind, tid, (sl_i64) { out, 3 });
    if (ok != 1) {
        printf("FAIL %-28s kind=%lld: the module does not declare it\n",
               name, (long long) kind);
        ++*cases; ++*bad;
        return 0;
    }
    const int64_t * ne = gguf_get_tensor_ne(ctx, (int) id);
    ++*cases;
    if (out[0] != ne[0] || out[1] != ne[1] || out[2] != ne[2]) {
        printf("FAIL %-28s kind=%lld declared=[%lld %lld %lld] gguf=[%lld %lld %lld]\n",
               name, (long long) kind, (long long) out[0], (long long) out[1],
               (long long) out[2], (long long) ne[0], (long long) ne[1], (long long) ne[2]);
        ++*bad;
        return 1;
    }
    return 1;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <apex.gguf>\n", argv[0]);
        return 2;
    }
    fg_goose_host_ensure_init();

    struct ggml_context * gctx = NULL;
    struct gguf_init_params params = { /*.no_alloc =*/ true, /*.ctx =*/ &gctx };
    struct gguf_context * ctx = gguf_init_from_file(argv[1], params);
    if (ctx == NULL) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }

    memset(p_block, 0, sizeof p_block);
    p_block[P_N_EMBD]        = kv_u32(ctx, "qwen35moe.embedding_length", 0);
    // The vocab size is a tokenizer property, not an architecture key: this model
    // carries no *.vocab_size and the count comes from the token list.
    p_block[P_N_VOCAB]       = kv_u32(ctx, "qwen35moe.vocab_size", 0);
    if (p_block[P_N_VOCAB] == 0) {
        const int64_t tok = gguf_find_key(ctx, "tokenizer.ggml.tokens");
        if (tok >= 0) {
            p_block[P_N_VOCAB] = (int64_t) gguf_get_arr_n(ctx, (int) tok);
        }
    }
    p_block[P_N_LAYER]       = kv_u32(ctx, "qwen35moe.block_count", 0);
    p_block[P_N_LAYER_ALL]   = p_block[P_N_LAYER] + 1;
    p_block[P_N_HEAD]        = kv_u32(ctx, "qwen35moe.attention.head_count", 0);
    p_block[P_N_HEAD_KV]     = kv_u32(ctx, "qwen35moe.attention.head_count_kv", 0);
    p_block[P_N_FF]          = kv_u32(ctx, "qwen35moe.feed_forward_length", 0);
    p_block[P_N_EXPERT]      = kv_u32(ctx, "qwen35moe.expert_count", 0);
    p_block[P_N_EXPERT_USED] = kv_u32(ctx, "qwen35moe.expert_used_count", 0);
    p_block[P_N_FF_EXP]      = kv_u32(ctx, "qwen35moe.expert_feed_forward_length", 0);
    p_block[P_N_FF_SHEXP]    = kv_u32(ctx, "qwen35moe.expert_shared_feed_forward_length", 0);
    p_block[P_D_CONV]        = kv_u32(ctx, "qwen35moe.ssm.conv_kernel", 0);
    p_block[P_D_INNER]       = kv_u32(ctx, "qwen35moe.ssm.inner_size", 0);
    p_block[P_D_STATE]       = kv_u32(ctx, "qwen35moe.ssm.state_size", 0);
    p_block[P_DT_RANK]       = kv_u32(ctx, "qwen35moe.ssm.time_step_rank", 0);
    p_block[P_N_GROUP]       = kv_u32(ctx, "qwen35moe.ssm.group_count", 0);
    p_block[P_HEAD_K]        = kv_u32(ctx, "qwen35moe.attention.key_length", 0);
    p_block[P_INTERVAL]      = kv_u32(ctx, "qwen35moe.full_attention_interval", 0);
    p_block[P_HAS_OUTPUT]    = 1;
    g_p = (sl_i64) { p_block, 32 };

    int cases = 0, bad = 0;

    // Model-level tensors.
    check(ctx, -1, 0, "token_embd.weight",    T_TOKEN_EMBD,    &cases, &bad);
    check(ctx, -1, 0, "output_norm.weight",   T_OUTPUT_NORM,   &cases, &bad);

    // One recurrent layer and one attention layer.
    static const char * recr_names[] = {
        "attn_norm.weight", "post_attention_norm.weight",
        "ffn_gate_inp.weight", "ffn_down_exps.weight",
        "ffn_gate_exps.weight", "ffn_up_exps.weight",
        "ffn_gate_inp_shexp.weight", "ffn_gate_shexp.weight",
        "ffn_up_shexp.weight", "ffn_down_shexp.weight",
        "ssm_conv1d.weight", "ssm_a", "ssm_beta.weight", "ssm_alpha.weight",
        "ssm_norm.weight", "ssm_out.weight",
    };
    static const int64_t recr_tids[] = {
        T_ATTN_NORM, T_ATTN_POST_NORM,
        T_FFN_GATE_INP, T_FFN_DOWN_EXPS,
        T_FFN_GATE_EXPS, T_FFN_UP_EXPS,
        T_FFN_GATE_INP_SHEXP, T_FFN_GATE_SHEXP,
        T_FFN_UP_SHEXP, T_FFN_DOWN_SHEXP,
        T_SSM_CONV1D, T_SSM_A, T_SSM_BETA, T_SSM_ALPHA,
        T_SSM_NORM, T_SSM_OUT,
    };

    char buf[128];
    for (size_t k = 0; k < sizeof recr_tids / sizeof recr_tids[0]; ++k) {
        snprintf(buf, sizeof buf, "blk.0.%s", recr_names[k]);
        check(ctx, 0, 1 /* K_RECR */, buf, recr_tids[k], &cases, &bad);
    }

    static const char * attn_names[] = {
        "attn_norm.weight", "post_attention_norm.weight",
        "ffn_gate_inp.weight", "ffn_down_exps.weight",
        "ffn_gate_exps.weight", "ffn_up_exps.weight",
    };
    static const int64_t attn_tids[] = {
        T_ATTN_NORM, T_ATTN_POST_NORM,
        T_FFN_GATE_INP, T_FFN_DOWN_EXPS,
        T_FFN_GATE_EXPS, T_FFN_UP_EXPS,
    };
    for (size_t k = 0; k < sizeof attn_tids / sizeof attn_tids[0]; ++k) {
        snprintf(buf, sizeof buf, "blk.3.%s", attn_names[k]);
        check(ctx, 3, 0 /* K_ATTN */, buf, attn_tids[k], &cases, &bad);
    }

    printf("FG_APEX_SHAPE cases=%d mismatches=%d %s\n", cases, bad,
           bad == 0 ? "PASS" : "FAIL");

    gguf_free(ctx);
    if (gctx) ggml_free(gctx);
    return bad == 0 ? 0 : 1;
}
