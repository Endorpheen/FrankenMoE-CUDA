#include <stdint.h>
#include <stddef.h>

enum dsa_kernel {
    DSA_PACK, DSA_METADATA, DSA_BOUND, DSA_SCORE, DSA_CUTOFF, DSA_COMPACT,
    DSA_REPLAY, DSA_FINALIZE, DSA_FALLBACK_SCORE, DSA_FALLBACK_TOP, DSA_KERNEL_COUNT
};

static const char *const dsa_kernel_names[DSA_KERNEL_COUNT] = {
    "goose_dsa_pack_power2", "goose_dsa_bound_metadata", "goose_dsa_bound_query",
    "goose_dsa_integer_score", "goose_dsa_score_cutoff", "goose_dsa_compact",
    "goose_dsa_replay", "goose_dsa_finalize", "goose_dsa_fallback_score", "goose_dsa_fallback_top"
};

struct dsa_calls {
    void *context;
    int (*zero)(void *, uint64_t, size_t);
    int (*launch)(void *, enum dsa_kernel, unsigned, unsigned, unsigned, void **);
};

struct dsa_io {
    uint64_t q, k, weights, mask, scores, ids, config, workspace;
    size_t workspace_bytes;
    int64_t cells, rows, key_stride, query_stride, weight_stride, mask_stride;
};

enum { DSA_TILE_ROWS = 512, DSA_FALLBACK_BLOCKS = 128 };

static int64_t dsa_tile_rows(int64_t rows) {
    return rows < DSA_TILE_ROWS ? rows : DSA_TILE_ROWS;
}

static size_t dsa_workspace_bytes(int64_t cells, int64_t rows) {
    if (cells <= 4096 || cells > 32768 || cells % 16 || rows < 1 || rows > 4096) return 0;
    rows = dsa_tile_rows(rows);
    const size_t query_heads = (size_t)rows * 64;
    return (size_t)cells * 260 + query_heads * 264 + (size_t)rows * 8212 + 16;
}

static uint64_t dsa_take(uint64_t *cursor, size_t bytes) {
    const uint64_t result = *cursor;
    *cursor += bytes;
    return result;
}

static int dsa_dispatch(struct dsa_calls calls, struct dsa_io io) {
    const size_t required = dsa_workspace_bytes(io.cells, io.rows);
    if (!required || io.workspace_bytes < required || !io.workspace || io.workspace % 16 ||
        UINT64_MAX - io.workspace < required || !io.q || !io.k || !io.mask || !io.weights ||
        !io.scores || !io.ids || !io.config || io.key_stride < 256 || io.key_stride % 2 ||
        io.query_stride != 32768 || io.weight_stride != 256 || io.mask_stride != io.cells * 2)
        return 0;

    uint64_t cursor = io.workspace;
    const int64_t tile_rows = dsa_tile_rows(io.rows);
    const int64_t query_heads = tile_rows * 64;
    const unsigned fallback_blocks = io.cells / 16 < DSA_FALLBACK_BLOCKS
        ? io.cells / 16 : DSA_FALLBACK_BLOCKS;
    uint64_t khi = dsa_take(&cursor, io.cells * 128), klo = dsa_take(&cursor, io.cells * 128);
    uint64_t qhi = dsa_take(&cursor, query_heads * 128), qlo = dsa_take(&cursor, query_heads * 128);
    uint64_t ks = dsa_take(&cursor, io.cells * 4), qs = dsa_take(&cursor, query_heads * 4);
    uint64_t norms = dsa_take(&cursor, query_heads * 4), maxima = dsa_take(&cursor, 16);
    uint64_t bounds = dsa_take(&cursor, tile_rows * 4), cutoffs = dsa_take(&cursor, tile_rows * 4);
    uint64_t band = dsa_take(&cursor, tile_rows * 4096), replay = dsa_take(&cursor, tile_rows * 4096);
    uint64_t counts = dsa_take(&cursor, tile_rows * 8), status = dsa_take(&cursor, tile_rows * 4);
    int32_t half = 1, full = 0;
    int64_t head_stride = 512, capacity = 1024;
    uint64_t unused = 0;

    if (calls.zero(calls.context, maxima, 16)) return -1;
#define DSA_LAUNCH(kernel, gx, gy, bx, ...) do { \
    void *args[] = { __VA_ARGS__ }; \
    if (calls.launch(calls.context, kernel, gx, gy, bx, args)) return -1; \
} while (0)
    DSA_LAUNCH(DSA_PACK, io.cells, 1, 32, &io.k, &khi, &klo, &ks, &io.key_stride, &half);
    DSA_LAUNCH(DSA_METADATA, io.cells, 1, 32, &io.k, &ks, &unused, &maxima, &io.key_stride, &half);
    for (int64_t first = 0; first < io.rows; first += tile_rows) {
        const int64_t rows = dsa_tile_rows(io.rows - first);
        const int64_t heads = rows * 64;
        uint64_t q = io.q + first * io.query_stride;
        uint64_t weights = io.weights + first * io.weight_stride;
        uint64_t mask = io.mask + first * io.mask_stride;
        uint64_t ids = io.ids + first * 8192;
        DSA_LAUNCH(DSA_PACK, heads, 1, 32, &q, &qhi, &qlo, &qs, &head_stride, &full);
        DSA_LAUNCH(DSA_METADATA, heads, 1, 32, &q, &qs, &norms, &maxima, &head_stride, &full);
        DSA_LAUNCH(DSA_BOUND, rows, 1, 32, &norms, &qs, &maxima, &weights, &bounds);
        DSA_LAUNCH(DSA_SCORE, io.cells / 16, rows, 256, &khi, &klo, &qhi, &qlo, &ks, &qs,
            &weights, &mask, &io.scores, &io.cells);
        DSA_LAUNCH(DSA_CUTOFF, rows, 1, 256, &io.scores, &cutoffs, &io.cells);
        DSA_LAUNCH(DSA_COMPACT, rows, 1, 256, &io.scores, &mask, &cutoffs, &bounds,
            &ids, &band, &counts, &io.cells, &capacity);
        DSA_LAUNCH(DSA_REPLAY, capacity / 4, rows, 128, &q, &io.k, &weights, &mask,
            &replay, &io.config, &band, &counts, &capacity);
        DSA_LAUNCH(DSA_FINALIZE, rows, 1, 256, &replay, &band, &counts, &ids, &status, &io.cells);
        DSA_LAUNCH(DSA_FALLBACK_SCORE, fallback_blocks, rows, 128, &q, &io.k, &weights,
            &mask, &io.scores, &io.config, &status);
        DSA_LAUNCH(DSA_FALLBACK_TOP, rows, 1, 256, &io.scores, &ids, &status, &io.cells);
    }
#undef DSA_LAUNCH
    return 1;
}
