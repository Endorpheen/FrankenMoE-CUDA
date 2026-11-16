// Apex plan gate: the Qwen3.5-MoE architecture builder over a real Apex model's
// metadata.
//
// This is a TEST. It reads a GGUF through the substrate reader, fills the flat
// parameter block goose/model/qwen35moe.goose documents, and asks the module for
// the per-layer plan. The output is the plan a graph builder would consume.
//
// It does not build a graph and does not start a model: it proves that the
// architecture builder answers for the actual weights file, not only for the
// synthetic parameter sets EXP-577 accepted.
//
// Parameter indices are the module's own (P_* in qwen35moe.goose).
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

extern void fg_goose_host_ensure_init(void);

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

    const char * arch = gguf_find_key(ctx, "general.architecture") >= 0
        ? gguf_get_val_str(ctx, (int) gguf_find_key(ctx, "general.architecture")) : "?";

    int64_t p[32] = { 0 };
    p[P_N_EMBD]        = kv_u32(ctx, "qwen35moe.embedding_length", 0);
    p[P_N_VOCAB]       = kv_u32(ctx, "qwen35moe.vocab_size", 0);
    // The MTP block is stored IN the file at index n_layer, so n_layer is block_count minus the nextn
    // count and NOT block_count. gdn_plan records the same rule; this harness had it wrong both ways in
    // turn (first n_layer+1, which invented a 31st recurrent layer, then block_count, which made the MTP
    // block look recurrent and reported mtp=0).
    const int64_t n_nextn = kv_u32(ctx, "qwen35moe.nextn_predict_layers", 0);
    p[P_N_LAYER]       = kv_u32(ctx, "qwen35moe.block_count", 0) - n_nextn;
    p[P_N_LAYER_ALL]   = p[P_N_LAYER] + n_nextn;
    p[P_N_HEAD]        = kv_u32(ctx, "qwen35moe.attention.head_count", 0);
    p[P_N_HEAD_KV]     = kv_u32(ctx, "qwen35moe.attention.head_count_kv", 0);
    p[P_N_FF]          = kv_u32(ctx, "qwen35moe.feed_forward_length", 0);
    p[P_N_EXPERT]      = kv_u32(ctx, "qwen35moe.expert_count", 0);
    p[P_N_EXPERT_USED] = kv_u32(ctx, "qwen35moe.expert_used_count", 0);
    p[P_N_FF_EXP]      = kv_u32(ctx, "qwen35moe.expert_feed_forward_length", 0);
    p[P_N_FF_SHEXP]    = kv_u32(ctx, "qwen35moe.expert_shared_feed_forward_length", 0);
    p[P_D_CONV]        = kv_u32(ctx, "qwen35moe.ssm.conv_kernel", 0);
    p[P_D_INNER]       = kv_u32(ctx, "qwen35moe.ssm.inner_size", 0);
    p[P_D_STATE]       = kv_u32(ctx, "qwen35moe.ssm.state_size", 0);
    p[P_DT_RANK]       = kv_u32(ctx, "qwen35moe.ssm.time_step_rank", 0);
    p[P_N_GROUP]       = kv_u32(ctx, "qwen35moe.ssm.group_count", 0);
    p[P_HEAD_K]        = kv_u32(ctx, "qwen35moe.attention.key_length", 0);
    // The loader's interval key is full_attention_interval: every Nth layer is
    // full attention and the rest are recurrent (GDN). Reading the older
    // recurrent_layers_interval name yielded 0 and a plan that was all attention.
    p[P_INTERVAL]      = kv_u32(ctx, "qwen35moe.full_attention_interval", 0);
    p[P_HAS_OUTPUT]    = 1;

    // n_layer_all is n_layer, NOT n_layer + 1. The MTP block is blk.40 in the file itself - it carries
    // nextn.eh_proj / enorm / hnorm / shared_head_norm - and no blk.41 exists, so the loader appends
    // nothing. The earlier "+1" produced 42 layers and a 31st recurrent layer that does not exist, which
    // gdn_plan and stack_dispatch both contradict. Verified by reading the tensor names out of the GGUF.


    int64_t kinds_storage[512];
    int64_t p_slice_storage[32];
    for (int i = 0; i < 32; ++i) p_slice_storage[i] = p[i];

    const sl_i64 pslice = { p_slice_storage, 32 };
    const sl_i64 kslice = { kinds_storage, 512 };
    const int64_t n = q35_plan(pslice, 0, kslice);
    if (n < 0 || n > 512) {
        printf("FG_APEX_PLAN rc=%lld arch=%s (the module refused the parameters)\n",
               (long long) n, arch);
        gguf_free(ctx);
        if (gctx) ggml_free(gctx);
        return 1;
    }

    int64_t n_attn = 0, n_recr = 0, n_mtp = 0, n_other = 0;
    for (int64_t i = 0; i < n; ++i) {
        switch (kinds_storage[i]) {
            case 0: ++n_attn; break;
            case 1: ++n_recr; break;
            case 2: ++n_mtp;  break;
            default: ++n_other; break;
        }
    }
    printf("FG_APEX_PLAN arch=%s layers=%lld attn=%lld recr=%lld mtp=%lld other=%lld "
           "embd=%lld head=%lld head_kv=%lld experts=%lld used=%lld interval=%lld\n",
           arch, (long long) n, (long long) n_attn, (long long) n_recr,
           (long long) n_mtp, (long long) n_other,
           (long long) p[P_N_EMBD], (long long) p[P_N_HEAD], (long long) p[P_N_HEAD_KV],
           (long long) p[P_N_EXPERT], (long long) p[P_N_EXPERT_USED],
           (long long) p[P_INTERVAL]);

    gguf_free(ctx);
    if (gctx) ggml_free(gctx);
    return (n_other == 0 && n > 0) ? 0 : 1;
}
