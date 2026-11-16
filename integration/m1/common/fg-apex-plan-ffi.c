#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ggml.h"

// The five FFI symbols qwen35moe.goose / qgate-split.goose need beyond their own extern lists,
// extracted from the standalone bindings so this object carries nothing else and clashes with nothing.

void fg158_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "Goose graph builder validation failed:code=%" PRId64, code);
}

int64_t fg158_view4(int64_t c, int64_t a, int64_t n0, int64_t n1, int64_t n2, int64_t n3,
                    int64_t nb1, int64_t nb2, int64_t nb3, int64_t offset) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * src = (struct ggml_tensor *) (intptr_t) a;
    if (ctx == NULL || src == NULL) {
        return 0;
    }
    return (int64_t) (intptr_t) ggml_view_4d(ctx, src, n0, n1, n2, n3,
                                             (size_t) nb1, (size_t) nb2, (size_t) nb3,
                                             (size_t) offset);
}

int64_t fg158_rope_multi(int64_t c, int64_t a, int64_t pos, int64_t n_rot,
                         int64_t s0, int64_t s1, int64_t s2, int64_t s3,
                         int64_t mode, int64_t n_ctx_orig, int64_t fb_bits,
                         int64_t fs_bits, int64_t ef_bits, int64_t af_bits,
                         int64_t bf_bits, int64_t bs_bits) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * src = (struct ggml_tensor *) (intptr_t) a;
    struct ggml_tensor  * p   = (struct ggml_tensor *) (intptr_t) pos;
    if (ctx == NULL || src == NULL) {
        return 0;
    }
    int sections[4] = { (int) s0, (int) s1, (int) s2, (int) s3 };
    float fb, fs, ef, af, bf, bs;
    int32_t b;
    b = (int32_t) fb_bits; memcpy(&fb, &b, sizeof fb);
    b = (int32_t) fs_bits; memcpy(&fs, &b, sizeof fs);
    b = (int32_t) ef_bits; memcpy(&ef, &b, sizeof ef);
    b = (int32_t) af_bits; memcpy(&af, &b, sizeof af);
    b = (int32_t) bf_bits; memcpy(&bf, &b, sizeof bf);
    b = (int32_t) bs_bits; memcpy(&bs, &b, sizeof bs);
    return (int64_t) (intptr_t) ggml_rope_multi(ctx, src, p, NULL, (int) n_rot,
        sections, (int) mode, (int) n_ctx_orig, fb, fs, ef, af, bf, bs);
}

void fg200g_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "model graph refused:code=%" PRId64, code);
}

int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    uint32_t v = 0;
    memcpy(&v, (const char *) (uintptr_t) base + index * (int64_t) sizeof v, sizeof v);
    return (int64_t) v;
}
