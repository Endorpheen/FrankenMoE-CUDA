// Bridge for the simple batch split (EXP351, op 0) into llama_batch_allocr::split_simple.
//
// The decision crosses as: the used flags and the size limit in, the taken batch indices and the
// resulting count out. The ubatch bookkeeping (out_ids, ubatch_add) stays in llama.
//
// The module writes its result as a record journal; the bridge reads the "taken" records back into
// the caller's index array. A capture of zero means "no free token", which llama reports as an empty
// ubatch.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;
// The first call is announced immediately: a SIGINT shutdown does not run destructors, and a witness
// that only prints at exit would look like "never called".
static void fg351s_tick(void){
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG351S_BATCH_SPLIT_SIMPLE first call\n");
    if ((n % 1000) == 0) fprintf(stderr, "FG351S_BATCH_SPLIT_SIMPLE calls=%llu\n", n);
}

static void g_fail(int64_t c){ fprintf(stderr, "FG351S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p, int64_t k, int64_t i, uint64_t v){ ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t) v; }
static uint64_t g_get(int64_t p, int64_t k, int64_t i){ return (uint64_t) ((const int64_t *)(uintptr_t)p)[k*16 + i]; }
static void   g_stw(uint64_t b, int64_t o, int64_t v){ *(int64_t *)(uintptr_t)(b + (uint64_t) o) = v; }
static int64_t g_ld(uint64_t b, int64_t o){ return *(const int64_t *)(uintptr_t)(b + (uint64_t) o); }

#define fg351k_get   g_get
#define fg351k_put   g_put
#define fg351k_ld_i64 g_ld
#define fg351k_stw   g_stw
#define fg351k_fail  g_fail
#define gg_init_globals fg351s_init_globals
#define main fg351s_generated_main
#include "host-qualified.c"
#undef main

// used: n_tokens flags (int32 in, int64 scratch built here)
// out_idxs: capacity >= n_ubatch
// returns the number of tokens taken (0 = nothing free)
int64_t fg351s_split_simple(int64_t n_tokens, int64_t n_ubatch, const int32_t * used, int32_t * out_idxs,
                            int64_t out_cap, int32_t * used_out) {
    fg351s_tick();

    int64_t * used64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n_tokens > 0 ? n_tokens : 1));
    if (!used64) abort();
    for (int64_t i = 0; i < n_tokens; ++i) used64[i] = used ? (used[i] ? 1 : 0) : 0;

    const int64_t cap = 4 * (n_tokens + 4);
    int64_t * journal = (int64_t *) malloc(sizeof(int64_t) * 3 * (size_t) cap);
    int64_t * cnt = (int64_t *) calloc(4, sizeof(int64_t));
    int64_t * pkt = (int64_t *) calloc(8 * 16, sizeof(int64_t));
    if (!journal || !cnt || !pkt) abort();

    pkt[0*16 + 0] = 0;                 // op: simple
    pkt[0*16 + 1] = n_tokens;
    pkt[0*16 + 2] = (int64_t)(uintptr_t) used64;
    pkt[0*16 + 3] = 0;                 // sequence sets are not used by the simple split
    pkt[0*16 + 4] = n_ubatch;
    pkt[1*16 + 0] = (int64_t)(uintptr_t) journal;
    pkt[1*16 + 1] = (int64_t)(uintptr_t) cnt;

    (void) fg351ds_gg_function_1((int64_t)(uintptr_t) pkt, 1);

    int64_t taken = 0;
    const int64_t n = cnt[0];
    for (int64_t i = 0; i < n; ++i) {
        if (journal[i*3] == 0 && taken < out_cap) {
            out_idxs[taken] = (int32_t) journal[i*3 + 1];
            ++taken;
        }
    }
    if (used_out) {
        for (int64_t i = 0; i < n_tokens; ++i) used_out[i] = (int32_t) used64[i];
    }

    free(pkt); free(cnt); free(journal); free(used64);
    return taken;
}

int fg351s_enabled(void) {
    const char * f = getenv("GGML_GOOSE_BATCH_SPLIT_SIMPLE");
    return !(f && strcmp(f, "0") == 0);
}
