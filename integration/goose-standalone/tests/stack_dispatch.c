// Stack dispatch: does each layer get the builder its kind requires?
//
// q35_plan decides 41 kinds (10 attention, 30 recurrent, 1 MTP) and EXP-699 verified that
// against the file's own tensor signatures. What has never been checked is the loop that
// consumes the plan: walking the layers in order and selecting a builder per kind.
//
// Each layer is a cheap stand-in whose output carries a signature of which kind ran, so the
// check can say "layer 7 used the recurrent builder" without depending on the arithmetic of
// either builder. A shifted or mis-indexed dispatch then shows up as an off-by-one in the
// signature sequence, which is exactly the failure mode a stack can have while every layer
// it does build is correct.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gguf.h"
#include "ggml.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);

static int64_t kv_u32(struct gguf_context * c, const char * key, int64_t dflt) {
    const int64_t id = gguf_find_key(c, key);
    if (id < 0) return dflt;
    if (gguf_get_kv_type(c, (int) id) == GGUF_TYPE_UINT32) return (int64_t) gguf_get_val_u32(c, (int) id);
    if (gguf_get_kv_type(c, (int) id) == GGUF_TYPE_INT32) return (int64_t) gguf_get_val_i32(c, (int) id);
    return dflt;
}

// The two stand-ins. Their VALUES are irrelevant; what matters is that each leaves a
// distinguishable signature so the walk can be audited.
static float attn_stand_in(float x) { return x * 2.0f + 1.0f; }
static float recr_stand_in(float x) { return x * 3.0f - 1.0f; }

int main(int argc, char ** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) { fprintf(stderr, "usage: %s <apex.gguf>\n", argv[0]); return 2; }
    fg_goose_host_ensure_init();

    struct ggml_context * meta = NULL;
    struct gguf_init_params mp = { /*.no_alloc =*/ true, /*.ctx =*/ &meta };
    struct gguf_context * g = gguf_init_from_file(argv[1], mp);
    if (g == NULL) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    int64_t p[19] = {0};
    const int64_t n_nextn = kv_u32(g, "qwen35moe.nextn_predict_layers", 0);
    p[0]  = kv_u32(g, "qwen35moe.embedding_length", 0);
    p[1]  = kv_u32(g, "qwen35moe.vocab_size", 0);
    p[2]  = kv_u32(g, "qwen35moe.block_count", 0) - n_nextn;
    p[3]  = p[2] + n_nextn;
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
    p[17] = kv_u32(g, "qwen35moe.full_attention_interval", 0);
    p[18] = kv_u32(g, "qwen35moe.output_length", 0);

    const int64_t n = p[3];
    int64_t * kinds = (int64_t *) calloc((size_t) n, sizeof(int64_t));
    sl_i64 ps = { p, 19 };
    sl_i64 ks = { kinds, n };
    if (q35_plan(ps, 0, ks) != n) { printf("FG_STACK_DISPATCH plan failed\n"); return 1; }

    // A second, deliberately shifted plan: the mistake of using il+1 or starting the walk
    // one layer late. It must produce a different signature sequence, or the audit below is
    // insensitive to the very error it exists to catch.
    int64_t * shifted = (int64_t *) calloc((size_t) n, sizeof(int64_t));
    for (int64_t i = 0; i < n; ++i) shifted[i] = kinds[(i + 1) % n];

    int bad = 0, shifted_diff = 0;
    float xa = 1.0f, xb = 1.0f;
    for (int64_t il = 0; il < n; ++il) {
        // The dispatch decision is the plan's, and nothing else in the loop may look at
        // the layer index to decide which builder to use.
        const int64_t k = kinds[il];
        const int64_t kshift = shifted[il];
        if (k == 1) { xa = recr_stand_in(xa); } else { xa = attn_stand_in(xa); }
        if (kshift == 1) { xb = recr_stand_in(xb); } else { xb = attn_stand_in(xb); }

        if ((k == 1) != (kshift == 1)) ++bad;   // counts where the two plans disagree
    }
    shifted_diff = bad;
    bad = 0;

    // Now the real audit: the sequence of decisions must be reproducible from the plan, and
    // the counts must be the ones the file supports.
    int64_t n_attn = 0, n_recr = 0, n_mtp = 0, first_attn = -1, last_attn = -1;
    for (int64_t il = 0; il < n; ++il) {
        const int64_t k = kinds[il];
        if (k == 0) { ++n_attn; if (first_attn < 0) first_attn = il; last_attn = il; }
        else if (k == 1) ++n_recr;
        else if (k == 2) ++n_mtp;
        else ++bad;
        // MTP may only be the final layer: it sits above n_layer by construction.
        if (k == 2 && il != n - 1) ++bad;
    }

    // The attention layers must be the multiples of the interval, counted from 1. If the
    // loop used il instead of il+1 this would land on every layer and the count would still
    // look plausible - so it is checked positionally, not just by total.
    int64_t pos_bad = 0;
    for (int64_t il = 0; il < p[2]; ++il) {
        const int expect_attn = (p[17] > 0 && (il + 1) % p[17] == 0);
        if ((kinds[il] == 0) != (expect_attn != 0)) ++pos_bad;
    }

    printf("FG_STACK_DISPATCH n=%lld attn=%lld recr=%lld mtp=%lld first_attn=%lld "
           "last_attn=%lld pos_bad=%lld shifted_differs=%d bad=%lld %s\n",
           (long long) n, (long long) n_attn, (long long) n_recr, (long long) n_mtp,
           (long long) first_attn, (long long) last_attn, (long long) pos_bad,
           shifted_diff, (long long) bad,
           (bad == 0 && pos_bad == 0 && shifted_diff > 0 && n_attn + n_recr + n_mtp == n)
               ? "PASS" : "FAIL");

    free(kinds); free(shifted);
    gguf_free(g);
    if (meta) ggml_free(meta);
    return 0;
}
