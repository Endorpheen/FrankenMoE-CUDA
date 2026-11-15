// q35_shape against the file.
//
// The module decides the shape of every tensor it knows about. The GGUF has the actual
// shapes. This asks both, for the tensors whose native name and shape are unambiguous in
// qwen35moe.cpp:73-105, and compares.
//
// Only the tensors whose mapping is read directly from that block are included. Anything
// whose name would have to be guessed is left out on purpose: a guessed mapping produces
// differences that are the gate's fault, and this project has already lost time to that.
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

// tid, name pattern, which kind's branch it belongs to (0 attn, 1 recr, 2 any)
typedef struct { int64_t tid; const char * fmt; int branch; } entry;

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

    static const entry tab[] = {
        {  3, "blk.%lld.attn_norm.weight",           2 },
        {  4, "blk.%lld.post_attention_norm.weight", 2 },
        {  6, "blk.%lld.attn_output.weight",         0 },
        {  7, "blk.%lld.attn_q_norm.weight",         0 },
        {  8, "blk.%lld.attn_k_norm.weight",         0 },
        { 11, "blk.%lld.ssm_conv1d.weight",          1 },
        { 12, "blk.%lld.ssm_dt.bias",                1 },
        { 13, "blk.%lld.ssm_a",                      1 },
        { 14, "blk.%lld.ssm_beta.weight",            1 },
        { 15, "blk.%lld.ssm_alpha.weight",           1 },
        { 16, "blk.%lld.ssm_norm.weight",            1 },
        { 17, "blk.%lld.ssm_out.weight",             1 },
        { 18, "blk.%lld.ffn_gate_inp.weight",        2 },
        { 19, "blk.%lld.ffn_down_exps.weight",       2 },
        { 21, "blk.%lld.ffn_gate_exps.weight",       2 },
        { 22, "blk.%lld.ffn_up_exps.weight",         2 },
        { 23, "blk.%lld.ffn_gate_inp_shexp.weight",  2 },
        { 24, "blk.%lld.ffn_gate_shexp.weight",      2 },
        { 25, "blk.%lld.ffn_up_shexp.weight",        2 },
        { 26, "blk.%lld.ffn_down_shexp.weight",      2 },
    };
    const size_t n_tab = sizeof tab / sizeof tab[0];

    // One layer of each kind: 0 is recurrent, 3 is full attention.
    const int64_t layers[2] = { 0, 3 };
    const int64_t kinds[2]  = { 1, 0 };

    int checked = 0, mismatch = 0, absent = 0;
    sl_i64 ps = { p, 19 };
    for (int li = 0; li < 2; ++li) {
        for (size_t i = 0; i < n_tab; ++i) {
            if (tab[i].branch != 2 && tab[i].branch != kinds[li]) continue;

            char name[96];
            snprintf(name, sizeof name, tab[i].fmt, (long long) layers[li]);
            const int64_t t = gguf_find_tensor(g, name);
            if (t < 0) { ++absent; continue; }

            int64_t want[3] = { 0, 0, 0 };
            sl_i64 out = { want, 3 };
            const int64_t r = q35_shape(ps, kinds[li], tab[i].tid, out);
            if (r <= 0) {
                ++mismatch;
                printf("  %s: q35_shape returned %lld\n", name, (long long) r);
                continue;
            }
            const int64_t * ne = gguf_get_tensor_ne(g, (int) t);
            // The module's answer is [n0, n1, n2] with 1 for absent dims; the file stores
            // actual extents and pads with 1, so they compare directly.
            if (want[0] != ne[0] || want[1] != ne[1] || want[2] != ne[2]) {
                ++mismatch;
                printf("  %s: goose [%lld %lld %lld] file [%lld %lld %lld]\n", name,
                       (long long) want[0], (long long) want[1], (long long) want[2],
                       (long long) ne[0], (long long) ne[1], (long long) ne[2]);
            }
            ++checked;
        }
    }

    printf("FG_Q35_SHAPE checked=%d mismatch=%d absent=%d %s\n",
           checked, mismatch, absent, (mismatch == 0 && checked > 0 && absent == 0) ? "PASS" : "FAIL");

    gguf_free(g);
    if (meta) ggml_free(meta);
    return 0;
}
