// state_slot must reproduce both earlier slots exactly.
//
// The point of unifying the two caches is that one function can be fixed once. That is
// only safe if the unified version returns the SAME tensor the specialised ones did - not
// an equivalent one - so this compares view shape, strides and offset against
// attn_kv_slot and gdn_conv_slot directly.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggml.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);
extern int64_t attn_kv_slot(int64_t c, int64_t cache, int64_t layer, int64_t pos,
                            int64_t head_dim, int64_t n_ctx, int64_t n_kv_heads,
                            int64_t n_layers, int64_t n_tokens, int64_t elem);
extern int64_t gdn_conv_slot(int64_t c, int64_t states_all, int64_t kernel, int64_t channels,
                             int64_t n_seqs, int64_t slot, int64_t mem_size, int64_t kv_head,
                             int64_t elem);
extern int64_t state_slot(int64_t c, int64_t cache, int64_t layout, int64_t layer,
                          int64_t pos, int64_t a, int64_t b, int64_t c2, int64_t n_layers,
                          int64_t n_tokens, int64_t elem);

static int same(struct ggml_tensor * x, struct ggml_tensor * y, const char * what, int64_t i) {
    if (!x || !y) {
        printf("  %s %lld: null (%p/%p)\n", what, (long long) i, (void *) x, (void *) y);
        return 0;
    }
    int ok = 1;
    for (int d = 0; d < 4; ++d) {
        if (x->ne[d] != y->ne[d] || x->nb[d] != y->nb[d]) ok = 0;
    }
    if (x->view_offs != y->view_offs) ok = 0;
    if (x->op != y->op) ok = 0;
    if (!ok) {
        printf("  %s %lld: ne[%lld %lld %lld %lld] nb[%zu %zu %zu %zu] off=%zu\n", what,
               (long long) i,
               (long long) x->ne[0], (long long) x->ne[1], (long long) x->ne[2], (long long) x->ne[3],
               x->nb[0], x->nb[1], x->nb[2], x->nb[3], (size_t) x->view_offs);
        printf("       vs ne[%lld %lld %lld %lld] nb[%zu %zu %zu %zu] off=%zu\n",
               (long long) y->ne[0], (long long) y->ne[1], (long long) y->ne[2], (long long) y->ne[3],
               y->nb[0], y->nb[1], y->nb[2], y->nb[3], (size_t) y->view_offs);
    }
    return ok;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    fg_goose_host_ensure_init();

    const size_t elem = sizeof(float);
    const int64_t head_dim = 256, n_ctx = 64, n_kv_heads = 2, n_layers = 4;
    const int64_t kernel = 4, channels = 8192, mem_size = 2, conv_layers = 3;

    struct ggml_init_params gp = { 256u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) { printf("FG_STATE_SLOT init failed\n"); return 2; }

    struct ggml_tensor * kv = ggml_new_tensor_4d(gc, GGML_TYPE_F32, head_dim, n_ctx, n_kv_heads, n_layers);
    struct ggml_tensor * cv = ggml_new_tensor_1d(gc, GGML_TYPE_F32,
        conv_layers * mem_size * (kernel - 1) * channels);

    int bad = 0, checks = 0;

    // layout 0: attention KV, every layer and several positions
    for (int64_t il = 0; il < n_layers; ++il) {
        for (int64_t pos = 0; pos < 4; ++pos) {
            struct ggml_tensor * a = (struct ggml_tensor *) (uintptr_t) attn_kv_slot(
                (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) kv, il, pos, head_dim, n_ctx,
                n_kv_heads, n_layers, 1, (int64_t) elem);
            struct ggml_tensor * b = (struct ggml_tensor *) (uintptr_t) state_slot(
                (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) kv, 0, il, pos, head_dim,
                n_ctx, n_kv_heads, n_layers, 1, (int64_t) elem);
            if (!same(a, b, "kv", il * 10 + pos)) ++bad;
            ++checks;
        }
    }

    // layout 1: recurrent conv, every layer and every slot
    for (int64_t il = 0; il < conv_layers; ++il) {
        for (int64_t kvh = 0; kvh < mem_size; ++kvh) {
            struct ggml_tensor * a = (struct ggml_tensor *) (uintptr_t) gdn_conv_slot(
                (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) cv, kernel, channels, 1,
                il, mem_size, kvh, (int64_t) elem);
            struct ggml_tensor * b = (struct ggml_tensor *) (uintptr_t) state_slot(
                (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) cv, 1, il, kvh, kernel,
                channels, mem_size, conv_layers, 1, (int64_t) elem);
            if (!same(a, b, "conv", il * 10 + kvh)) ++bad;
            ++checks;
        }
    }

    // The range checks refuse by aborting, so they cannot be probed from here: calling
    // state_slot with pos == n_ctx terminates the process through fg158_fail with code
    // 103, which is the refusal. That behaviour is checked separately by running
    // `state_slot --overrange`, which must exit non-zero and print code=103.
    if (getenv("FG_STATE_SLOT_OVERRANGE") != NULL) {
        (void) state_slot((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) kv, 0,
                          0, n_ctx, head_dim, n_ctx, n_kv_heads, n_layers, 1,
                          (int64_t) elem);
        printf("FG_STATE_SLOT OVERRANGE_NOT_REFUSED\n");
        ggml_free(gc);
        return 3;
    }

    printf("FG_STATE_SLOT checks=%d bad=%d %s\n",
           checks, bad, (bad == 0 && checks == 22) ? "PASS" : "FAIL");

    ggml_free(gc);
    return 0;
}
