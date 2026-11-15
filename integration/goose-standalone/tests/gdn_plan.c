// Layer-kind plan gate: q35_plan over the real Apex parameters.
//
// The stack needs to know which of the layers are recurrent before any of it can be
// built. llama-hparams.cpp:24-28 gives the rule: use the per-layer u32 array when the
// GGUF carries it, otherwise derive from full_attention_interval as
//
//     is_recr[i] = (i < n_layer) && ((i + 1) % interval != 0)
//
// and the MTP layer sits above n_layer. q35_kind already implements that rule; this
// gate checks that it produces the distribution the model actually has, rather than
// trusting the formula's shape.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gguf.h"
#include "ggml.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);

// Goose exports take slices, not raw pointers: sl_i64 is {data, len} and is already
// declared by the generated main.h.

static int64_t kv_u32(struct gguf_context * c, const char * key, int64_t dflt) {
    const int64_t id = gguf_find_key(c, key);
    if (id < 0) return dflt;
    if (gguf_get_kv_type(c, (int) id) == GGUF_TYPE_UINT32) return (int64_t) gguf_get_val_u32(c, (int) id);
    if (gguf_get_kv_type(c, (int) id) == GGUF_TYPE_INT32) return (int64_t) gguf_get_val_i32(c, (int) id);
    return dflt;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <apex.gguf>\n", argv[0]);
        return 2;
    }
    fg_goose_host_ensure_init();

    struct ggml_context * meta = NULL;
    struct gguf_init_params mp = { /*.no_alloc =*/ true, /*.ctx =*/ &meta };
    struct gguf_context * g = gguf_init_from_file(argv[1], mp);
    if (g == NULL) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }

    int64_t p[19] = {0};
    p[0]  = kv_u32(g, "qwen35moe.embedding_length", 0);              // n_embd
    p[1]  = kv_u32(g, "qwen35moe.vocab_size", 0);                    // n_vocab
    p[2]  = kv_u32(g, "qwen35moe.block_count", 0);                   // n_layer
    p[3]  = p[2] + kv_u32(g, "qwen35moe.nextn_predict_layers", 0);   // n_layer_all
    p[4]  = kv_u32(g, "qwen35moe.attention.head_count", 0);
    p[5]  = kv_u32(g, "qwen35moe.attention.head_count_kv", 0);
    p[6]  = kv_u32(g, "qwen35moe.feed_forward_length", 0);
    p[7]  = kv_u32(g, "qwen35moe.expert_count", 0);
    p[8]  = kv_u32(g, "qwen35moe.expert_used_count", 0);
    p[9]  = kv_u32(g, "qwen35moe.expert_feed_forward_length", 0);
    p[10] = kv_u32(g, "qwen35moe.expert_shared_feed_forward_length", 0);
    p[11] = kv_u32(g, "qwen35moe.ssm.conv_kernel", 0);
    p[12] = kv_u32(g, "qwen35moe.ssm.inner_size", 0);
    p[13] = kv_u32(g, "qwen35moe.ssm.state_size", 0);
    p[14] = kv_u32(g, "qwen35moe.ssm.time_step_rank", 0);
    p[15] = kv_u32(g, "qwen35moe.ssm.group_count", 0);
    p[16] = kv_u32(g, "qwen35moe.attention.key_length", 0);
    p[17] = kv_u32(g, "qwen35moe.full_attention_interval", 0);       // interval
    p[18] = kv_u32(g, "qwen35moe.output_length", 0);                 // has_output

    int64_t * kinds = (int64_t *) calloc((size_t) p[3], sizeof(int64_t));
    // flags_base 0 selects the interval formula, which is what this GGUF needs: it
    // carries no per-layer recurrent array.
    sl_i64 ps = { p, 19 };
    sl_i64 ks = { kinds, p[3] };
    const int64_t n = q35_plan(ps, 0, ks);

    int n_attn = 0, n_recr = 0, n_mtp = 0, n_bad = 0;
    printf("layers:");
    for (int64_t i = 0; i < n; ++i) {
        printf(" %lld", (long long) kinds[i]);
        if (kinds[i] == 0) ++n_attn;
        else if (kinds[i] == 1) ++n_recr;
        else if (kinds[i] == 2) ++n_mtp;
        else ++n_bad;
    }
    printf("\n");

    // Independent recomputation of the native rule, so the gate is not just the module
    // agreeing with itself.
    int e_attn = 0, e_recr = 0, e_mtp = 0;
    for (int64_t i = 0; i < p[3]; ++i) {
        if (i >= p[2]) { ++e_mtp; continue; }
        if (p[17] > 0 && (i + 1) % p[17] == 0) ++e_attn;
        else ++e_recr;
    }

    printf("FG_GDN_PLAN n=%lld attn=%d recr=%d mtp=%d bad=%d | expected %d/%d/%d %s\n",
           (long long) n, n_attn, n_recr, n_mtp, n_bad, e_attn, e_recr, e_mtp,
           (n == p[3] && n_bad == 0 && n_attn == e_attn && n_recr == e_recr &&
            n_mtp == e_mtp && n_recr > 0 && n_attn > 0) ? "PASS" : "FAIL");

    free(kinds);
    gguf_free(g);
    if (meta) ggml_free(meta);
    return 0;
}
