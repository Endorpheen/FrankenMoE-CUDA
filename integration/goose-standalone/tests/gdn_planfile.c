// The layer plan against the file.
//
// q35_plan says what kind each of the 42 layers is. The GGUF says what is actually there.
// A recurrent layer carries `attn_qkv`; a full-attention layer carries `attn_q`. Those are
// different tensors in qwen35moe.cpp:87-88 and 47-95, so the presence of one or the other
// is an independent statement about the layer's kind - it does not come from the same
// interval formula the plan uses.
//
// This is the cheapest check that catches a plan which is self-consistent but disagrees
// with the weights it is supposed to describe.
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

int main(int argc, char ** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) { fprintf(stderr, "usage: %s <apex.gguf>\n", argv[0]); return 2; }
    fg_goose_host_ensure_init();

    struct ggml_context * meta = NULL;
    struct gguf_init_params mp = { /*.no_alloc =*/ true, /*.ctx =*/ &meta };
    struct gguf_context * g = gguf_init_from_file(argv[1], mp);
    if (g == NULL) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    int64_t p[19] = {0};
    p[0]  = kv_u32(g, "qwen35moe.embedding_length", 0);
    p[1]  = kv_u32(g, "qwen35moe.vocab_size", 0);
    // blk.40 carries the regular block tensors AND nextn.*: the MTP block sits at index
    // n_layer, so n_layer is block_count minus the nextn count.
    const int64_t n_nextn = kv_u32(g, "qwen35moe.nextn_predict_layers", 0);
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

    int64_t * kinds = (int64_t *) calloc((size_t) p[3], sizeof(int64_t));
    sl_i64 ps = { p, 19 };
    sl_i64 ks = { kinds, p[3] };
    const int64_t n = q35_plan(ps, 0, ks);
    if (n != p[3]) { printf("FG_GDN_PLANFILE plan returned %lld\n", (long long) n); return 1; }

    int mismatch = 0, missing_both = 0;
    for (int64_t il = 0; il < p[3]; ++il) {
        char qkv[64], qp[64];
        snprintf(qkv, sizeof qkv, "blk.%lld.attn_qkv.weight", (long long) il);
        snprintf(qp, sizeof qp, "blk.%lld.attn_q.weight", (long long) il);
        const int has_qkv = gguf_find_tensor(g, qkv) >= 0;
        const int has_q   = gguf_find_tensor(g, qp)  >= 0;

        // The file's own statement of the layer's kind, independent of the interval rule.
        int file_kind = -1;
        if (has_qkv && !has_q) file_kind = 1;        // recurrent
        else if (has_q && !has_qkv) file_kind = 0;   // full attention
        else if (!has_q && !has_qkv) { ++missing_both; continue; }

        // The MTP layer is dense attention-only (qwen35moe.cpp:22-23), so its file
        // signature is the attention one, not a third kind.
        const int64_t want = kinds[il] == 2 ? 0 : kinds[il];
        if (file_kind != want) {
            ++mismatch;
            printf("  layer %lld: plan=%lld file=%d (qkv=%d q=%d)\n",
                   (long long) il, (long long) kinds[il], file_kind, has_qkv, has_q);
        }
    }

    // And the reverse direction: the MTP layer must be the plan's last entry and there
    // must be exactly one.
    int mtp = 0;
    for (int64_t i = 0; i < p[3]; ++i) if (kinds[i] == 2) ++mtp;

    printf("FG_GDN_PLANFILE layers=%lld mismatch=%d missing_both=%d mtp=%d last_kind=%lld %s\n",
           (long long) p[3], mismatch, missing_both, mtp, (long long) kinds[p[3] - 1],
           (mismatch == 0 && missing_both == 0 && mtp == 1 && kinds[p[3] - 1] == 2)
               ? "PASS" : "FAIL");

    free(kinds);
    gguf_free(g);
    if (meta) ggml_free(meta);
    return 0;
}
