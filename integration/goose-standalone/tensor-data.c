// Data loading for the standalone runtime. See tensor-data.h.
//
// The Goose side owns every number: nbytes() derives a tensor's byte size from
// its type traits, layout() derives the absolute offsets inside the data section
// and the padded total. This file only records what the substrate reader placed
// and checks it against the Goose answer.
//
// Why the check matters: ggml lays the payload out itself when it materialises the
// context. If its placement and the Goose layout disagree, one of the two is
// wrong, and the difference would otherwise appear as a wrong matmul much later.
// So the loader reports `mismatches` explicitly and the smoke host fails on a
// non-zero value.

#include "tensor-data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int64_t * data; int64_t len; } sl_i64;

// The composed unit is C; these are plain C declarations.
int64_t layout(sl_i64 rel, sl_i64 nb, int64_t align, int64_t data_start, sl_i64 out);
int64_t align_ok(int64_t a);

#define kMaxTensors 4096

int fg_goose_tensor_load(const struct gguf_context * ctx,
                                    struct ggml_context * gctx,
                                    fg_tensor_arena * out) {
    if (ctx == NULL || gctx == NULL || out == NULL) {
        return -2;
    }
    memset(out, 0, sizeof *out);

    const int64_t n = (int64_t) gguf_get_n_tensors(ctx);
    if (n <= 0) {
        return 0;   // a vocab-only file has no payload; not an error
    }
    if (n > kMaxTensors) {
        return -2;
    }

    const int64_t align = (int64_t) gguf_get_alignment(ctx);
    if (align_ok(align) != 1) {
        return -1;  // the module's own rule, not a host guess
    }
    const int64_t data_start = (int64_t) gguf_get_data_offset(ctx);

    static int64_t rel[kMaxTensors];
    static int64_t nb[kMaxTensors];
    static int64_t off_out[kMaxTensors + 1];

    for (int64_t i = 0; i < n; ++i) {
        rel[i] = (int64_t) gguf_get_tensor_offset(ctx, (int) i);
        nb[i]  = (int64_t) gguf_get_tensor_size(ctx, (int) i);
    }

    const int64_t rc = layout((sl_i64){ rel, n }, (sl_i64){ nb, n }, align, data_start,
                              (sl_i64){ off_out, n + 1 });
    out->layout_rc = rc;
    if (rc != 0) {
        return -1;
    }
    out->total = (size_t) off_out[n];

    void ** data = (void **) calloc((size_t) n, sizeof(void *));
    size_t * size = (size_t *) calloc((size_t) n, sizeof(size_t));
    if (data == NULL || size == NULL) {
        free(data);
        free(size);
        return -2;
    }

    const uint8_t * base = (const uint8_t *) ggml_get_mem_buffer(gctx);
    size_t copied = 0;
    int64_t mismatches = 0;
    // ggml reports tensor data relative to its own context buffer, while layout()
    // reports absolute file offsets, so the two differ by one constant. Compare
    // deltas: if the layouts agree, every delta agrees.
    int64_t first_delta = 0;
    bool have_delta = false;

    for (int64_t i = 0; i < n; ++i) {
        const char * name = gguf_get_tensor_name(ctx, (int) i);
        struct ggml_tensor * t = ggml_get_tensor(gctx, name);
        if (t == NULL) {
            continue;
        }
        data[i] = t->data;
        size[i] = ggml_nbytes(t);
        copied += size[i];

        // ggml's own placement versus the Goose layout, compared by delta.
        if (base != NULL) {
            const int64_t ggml_rel = (int64_t) ((const uint8_t *) t->data - base);
            const int64_t delta = ggml_rel - off_out[i];
            if (!have_delta) {
                first_delta = delta;
                have_delta = true;
            } else if (delta != first_delta) {
                ++mismatches;
            }
        }
    }

    out->data       = data;
    out->size       = size;
    out->n_tensors  = n;
    out->copied     = copied;
    out->mismatches = mismatches;
    return 0;
}

void fg_goose_tensor_free(fg_tensor_arena * a) {
    if (a == NULL) {
        return;
    }
    // The payload belongs to the reader's context, not to this struct.
    free(a->data);
    free(a->size);
    memset(a, 0, sizeof *a);
}
