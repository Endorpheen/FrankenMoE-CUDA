// Bridge for the split_equal tail policy (EXP348) into llama_batch_allocr::split_equal.
//
// The module reports what it did as a record journal; this adapter turns that journal into the three
// actions llama needs and applies the two states it owns (the used flags and the per-set cursors).
//
//   extend    push one index onto set 0's list, mark it used, ++n_used, ++cur_idx[0]
//   truncate  pop the last one back, clear it, --n_used, --cur_idx[0]
//   release   clear one index of a deferred set, --n_used
//
// Returns the number of sets kept; a zero-tail batch returns -1 and nothing is touched.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;
static void fg348s_tick(void){
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG348S_BATCH_SPLIT_TAIL first call\n");
    if ((n % 1000) == 0) fprintf(stderr, "FG348S_BATCH_SPLIT_TAIL calls=%llu\n", n);
}

static void g_fail(int64_t c){ fprintf(stderr, "FG348S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p, int64_t k, int64_t i, uint64_t v){ ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t) v; }
static uint64_t g_get(int64_t p, int64_t k, int64_t i){ return (uint64_t) ((const int64_t *)(uintptr_t)p)[k*16 + i]; }
static void   g_stw(uint64_t b, int64_t o, int64_t v){ *(int64_t *)(uintptr_t)(b + (uint64_t) o) = v; }
static int64_t g_ld(uint64_t b, int64_t o){ return *(const int64_t *)(uintptr_t)(b + (uint64_t) o); }

#define fg348k_get   g_get
#define fg348k_put   g_put
#define fg348k_ld_i64 g_ld
#define fg348k_stw   g_stw
#define fg348k_fail  g_fail
#define gg_init_globals fg348s_init_globals
#define main fg348s_generated_main
#include "host-qualified.c"
#undef main

// ext/tr/rel: action buffers, decided by the journal (capacity checked by the caller)
// returns n_keep, or -1 when the block does not apply
int64_t fg348s_keep_tail(int64_t n_seqs, const int32_t * sizes, int32_t * cur, const int32_t * idxs,
                         const int32_t * off, int32_t * used, int64_t used_len, int32_t * n_used_inout,
                         int64_t n_ubatch, int64_t n_keep_tail,
                         int32_t * ext, int64_t * ext_n, int32_t * tr, int64_t * tr_n,
                         int32_t * rel, int64_t * rel_n) {
    if (n_keep_tail <= 0) return -1;
    fg348s_tick();

    int64_t * sz64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n_seqs > 0 ? n_seqs : 1));
    int64_t * cu64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n_seqs > 0 ? n_seqs : 1));
    int64_t * ix64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(off[n_seqs] > 0 ? off[n_seqs] : 1));
    int64_t * of64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n_seqs + 1));
    if (!sz64 || !cu64 || !ix64 || !of64) abort();
    for (int64_t s = 0; s < n_seqs; ++s) { sz64[s] = sizes[s]; cu64[s] = cur[s]; }
    for (int64_t i = 0; i < off[n_seqs]; ++i) ix64[i] = idxs[i];
    for (int64_t s = 0; s <= n_seqs; ++s) of64[s] = off[s];

    // used: the module writes it, so it needs an int64 copy both ways, at the caller's length
    const int64_t n_used_slots = used_len;
    int64_t * us64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n_used_slots > 0 ? n_used_slots : 1));
    if (!us64) abort();
    for (int64_t i = 0; i < n_used_slots; ++i) us64[i] = used ? (used[i] ? 1 : 0) : 0;

    const int64_t cap = 4 * (n_seqs + 4) + 2 * off[n_seqs];
    int64_t * journal = (int64_t *) malloc(sizeof(int64_t) * 3 * (size_t) cap);
    int64_t * cnt = (int64_t *) calloc(4, sizeof(int64_t));
    int64_t * pkt = (int64_t *) calloc(8 * 16, sizeof(int64_t));
    if (!journal || !cnt || !pkt) abort();

    pkt[0*16 + 0] = n_seqs;
    pkt[0*16 + 1] = (int64_t)(uintptr_t) sz64;
    pkt[0*16 + 2] = (int64_t)(uintptr_t) cu64;
    pkt[0*16 + 3] = (int64_t)(uintptr_t) ix64;
    pkt[0*16 + 4] = (int64_t)(uintptr_t) of64;
    pkt[0*16 + 5] = (int64_t)(uintptr_t) us64;
    pkt[0*16 + 6] = n_used_inout ? (int64_t) *n_used_inout : 0;
    pkt[0*16 + 7] = n_ubatch;
    pkt[0*16 + 8] = n_keep_tail;
    pkt[1*16 + 0] = (int64_t)(uintptr_t) journal;
    pkt[1*16 + 1] = (int64_t)(uintptr_t) cnt;

    (void) fg348ds_gg_function_1((int64_t)(uintptr_t) pkt, 1);

    int64_t n_keep = -1;
    *ext_n = 0; *tr_n = 0; *rel_n = 0;
    // the module keeps its own counter; the caller's derives from the actions it reported
    const int64_t n_used0 = n_used_inout ? (int64_t) *n_used_inout : 0;
    const int64_t n = cnt[0];
    for (int64_t i = 0; i < n; ++i) {
        const int64_t kind = journal[i*3];
        const int64_t a = journal[i*3 + 1];
        const int64_t b = journal[i*3 + 2];
        if (kind == 0) { ext[(*ext_n)++] = (int32_t) b; }
        else if (kind == 1) { tr[(*tr_n)++] = (int32_t) b; }
        else if (kind == 2) { rel[(*rel_n)++] = (int32_t) b; }
        else if (kind == 4) { n_keep = a; }
    }

    // apply the states the module owns
    if (cur) for (int64_t s = 0; s < n_seqs; ++s) cur[s] = (int32_t) cu64[s];
    if (used) for (int64_t i = 0; i < n_used_slots; ++i) used[i] = (int32_t) us64[i];
    if (n_used_inout) *n_used_inout = (int32_t) (n_used0 + *ext_n - *tr_n - *rel_n);

    free(pkt); free(cnt); free(journal); free(us64); free(of64); free(ix64); free(cu64); free(sz64);
    return n_keep;
}

int fg348s_enabled(void) {
    const char * f = getenv("GGML_GOOSE_BATCH_SPLIT_TAIL");
    return !(f && strcmp(f, "0") == 0);
}
