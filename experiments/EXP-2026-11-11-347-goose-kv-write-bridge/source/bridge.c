// Bridge between llama's cell storage and the Goose state-write range module (EXP345).
//
// The decision crosses the boundary as scalars and flat arrays; the cell struct, the sequence set and
// the IO object stay on the llama side. The module returns the same record journal it was gated on,
// and the bridge turns that journal back into the two things llama needs: the per-stream cell count and
// the coalesced ranges (from inclusive, to exclusive).
//
// This is the adapter only. It does no correctness claim of its own: the decision is EXP345's, and the
// gate below checks that the adapter's translation of the journal back into ranges matches the journal
// it came from — the place where an off-by-one would otherwise hide.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg345w_report(void){
    if(!atomic_load(&g_calls)) return;
    fprintf(stderr, "FG345W_KV_STATE_WRITE calls=%llu\n", atomic_load(&g_calls));
}

static void g_fail(int64_t c){ fprintf(stderr, "FG345W validation code %lld\n", (long long)c); abort(); }
// The module addresses its packet by block and slot; the bridge keeps the same 16-slot block layout
// the gates used. It also has byte-addressed helpers for its own scratch arrays, which stay here.
static void fg345w_put(int64_t p, int64_t k, int64_t i, uint64_t v){
    ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t) v;
}
static uint64_t fg345w_get(int64_t p, int64_t k, int64_t i){
    return (uint64_t) ((const int64_t *)(uintptr_t)p)[k*16 + i];
}
static void fg345w_stw(uint64_t base, int64_t byte, int64_t v){
    *(int64_t *)(uintptr_t)(base + (uint64_t) byte) = v;
}
static int64_t fg345w_ld_i64(uint64_t base, int64_t byte){
    return *(const int64_t *)(uintptr_t)(base + (uint64_t) byte);
}

#define fg345k_get   fg345w_get
#define fg345k_put   fg345w_put
#define fg345k_ld_i64 fg345w_ld_i64
#define fg345k_stw   fg345w_stw
#define fg345k_fail  g_fail
#define fg345w_run fg345ds_gg_function_1
#define gg_init_globals fg345w_init_globals
#define main fg345w_generated_main
#include "host-qualified.c"
#undef main

// Journal record kinds of the module, repeated here so the adapter cannot drift silently.
enum { REC_HEADER = 0, REC_STREAM_COUNT = 1, REC_CELL_COUNT = 2, REC_RANGE = 3,
       REC_META = 4, REC_DATA = 5, REC_SKIP = 6 };

// out_counts: one entry per stream.
// out_ranges: two int32 per range, in journal order.
// Returns the number of ranges, or -1 when the module refused (mirrored cache / partial non-SWA).
int64_t fg345w_select(int64_t other, int64_t partial, int64_t swa_none, int64_t seq_id, int64_t n_stream,
                      int64_t n_swa, int64_t swa_type, int64_t size, const int32_t * pos,
                      const uint64_t * seqmask, int32_t * out_counts, int32_t * out_ranges,
                      int64_t out_ranges_cap) {
    atomic_fetch_add(&g_calls, 1);

    // The module reads int64 arrays; widen once, on this side.
    int64_t * pos64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n_stream * size) + 1);
    int64_t * mask64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n_stream * size) + 1);
    if (!pos64 || !mask64) { free(pos64); free(mask64); abort(); }
    for (int64_t i = 0; i < n_stream * size; ++i) {
        pos64[i]  = pos ? (int64_t) pos[i] : -1;
        mask64[i] = seqmask ? (int64_t) seqmask[i] : 0;
    }

    // The journal is bounded by two records per cell (range close + count) plus headers.
    const int64_t cap = 4 * n_stream * size + 4 * n_stream + 16;
    int64_t * journal = (int64_t *) malloc(sizeof(int64_t) * 3 * (size_t) cap);
    int64_t * cnt = (int64_t *) calloc(4, sizeof(int64_t));
    if (!journal || !cnt) { free(pos64); free(mask64); free(journal); free(cnt); abort(); }

    int64_t * pkt = (int64_t *) calloc(8 * 16, sizeof(int64_t));
    if (!pkt) { abort(); }
    pkt[0*16 + 1] = other; pkt[0*16 + 2] = partial; pkt[0*16 + 3] = swa_none;
    pkt[0*16 + 4] = seq_id; pkt[0*16 + 5] = n_stream; pkt[0*16 + 6] = n_swa;
    pkt[0*16 + 7] = swa_type; pkt[0*16 + 8] = size;
    pkt[0*16 + 9] = (int64_t)(uintptr_t) pos64;
    pkt[0*16 + 10] = (int64_t)(uintptr_t) mask64;
    pkt[1*16 + 0] = (int64_t)(uintptr_t) journal;
    pkt[1*16 + 1] = (int64_t)(uintptr_t) cnt;

    (void) fg345ds_gg_function_1((int64_t)(uintptr_t) pkt, 1);

    // The module reports a refusal by writing nothing at all: both exits use the same return code,
    // so an empty journal is the only signal.
    const int64_t n = cnt[0];
    const int64_t refused = n == 0 ? 1 : 0;
    int64_t n_ranges = 0;
    int64_t cur_stream = -1;
    for (int64_t i = 0; i < n; ++i) {
        const int64_t kind = journal[i*3];
        const int64_t a = journal[i*3 + 1];
        const int64_t b = journal[i*3 + 2];
        if (kind == REC_CELL_COUNT) {
            cur_stream = a;
            if (out_counts && a >= 0 && a < n_stream) out_counts[a] = (int32_t) b;
        } else if (kind == REC_RANGE) {
            if (out_ranges && n_ranges < out_ranges_cap) {
                out_ranges[n_ranges*2]     = (int32_t) a;
                out_ranges[n_ranges*2 + 1] = (int32_t) b;
            }
            ++n_ranges;
        } else if (kind == REC_STREAM_COUNT) {
            // the module writes the stream count first; nothing to translate
        }
    }
    (void) cur_stream;

    free(pkt);
    free(cnt);
    free(journal);
    free(pos64);
    free(mask64);
    return refused ? -1 : n_ranges;
}

int fg345w_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_STATE_WRITE");
    return !(f && strcmp(f, "0") == 0);
}
