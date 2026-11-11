// Bridge for the WORKING candidate's ubatch_add (EXP357).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;
static void fg357s_tick(void){
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG357S_UBATCH_ADD first call calls=1\n");
    if ((n % 10000) == 0) fprintf(stderr, "FG357S_UBATCH_ADD calls=%llu\n", n);
}
static void g_fail(int64_t c){ fprintf(stderr, "FG357S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p, int64_t k, int64_t i, uint64_t v){ ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t) v; }
static uint64_t g_get(int64_t p, int64_t k, int64_t i){ return (uint64_t) ((const int64_t *)(uintptr_t)p)[k*16 + i]; }
static void   g_stw(uint64_t b, int64_t o, int64_t v){ *(int64_t *)(uintptr_t)(b + (uint64_t) o) = v; }
static int64_t g_ld(uint64_t b, int64_t o){ return *(const int64_t *)(uintptr_t)(b + (uint64_t) o); }

#define fg357k_get   g_get
#define fg357k_put   g_put
#define fg357k_ld_i64 g_ld
#define fg357k_stw   g_stw
#define fg357k_fail  g_fail
#define gg_init_globals fg357s_init_globals
#define main fg357s_generated_main
#include "host-qualified.c"
#undef main

// returns 0; fills sizes[2], row_nseqid[n], row_output[n], out_ids[n], unq[n_seq_max] (or -1)
int64_t fg357s_ubatch(int64_t n, int64_t n_batch, const int32_t * idxs, const int32_t * logits,
                      const int32_t * nseqid, const int32_t * seqid, int64_t n_seq_max,
                      int64_t has_embd, int64_t n_embd, int64_t n_pos, int32_t * sizes,
                      int32_t * row_nseqid, int32_t * row_output, int32_t * out_ids, int32_t * out_ids_n,
                      int32_t * unq) {
    fg357s_tick();
    const int64_t nb = n_batch > 0 ? n_batch : 1;
    int64_t * ix = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n > 0 ? n : 1));
    int64_t * lg = (int64_t *) malloc(sizeof(int64_t) * (size_t) nb);
    int64_t * ns = (int64_t *) malloc(sizeof(int64_t) * (size_t) nb);
    int64_t * sq = (int64_t *) malloc(sizeof(int64_t) * (size_t)(nb*4));
    if (!ix || !lg || !ns || !sq) abort();
    for (int64_t i = 0; i < n; ++i) ix[i] = idxs[i];
    for (int64_t i = 0; i < nb; ++i) {
        lg[i] = logits ? logits[i] : 0;
        ns[i] = nseqid ? nseqid[i] : 1;
        for (int64_t j = 0; j < 4; ++j) sq[i*4 + j] = seqid ? seqid[i*4 + j] : 0;
    }

    const int64_t cap = 4*n + 8*n_seq_max + 32;
    int64_t * jr = (int64_t *) malloc(sizeof(int64_t) * 3 * (size_t) cap);
    int64_t * cnt = (int64_t *) calloc(4, sizeof(int64_t));
    int64_t * pkt = (int64_t *) calloc(8*16, sizeof(int64_t));
    if (!jr || !cnt || !pkt) abort();
    pkt[0*16 + 0] = n; pkt[0*16 + 1] = (int64_t)(uintptr_t) ix; pkt[0*16 + 2] = (int64_t)(uintptr_t) lg;
    pkt[0*16 + 3] = (int64_t)(uintptr_t) ns; pkt[0*16 + 4] = (int64_t)(uintptr_t) sq;
    pkt[0*16 + 5] = n_seq_max; pkt[0*16 + 6] = has_embd; pkt[0*16 + 7] = n_embd; pkt[0*16 + 8] = n_pos;
    pkt[1*16 + 0] = (int64_t)(uintptr_t) jr; pkt[1*16 + 1] = (int64_t)(uintptr_t) cnt;

    (void) fg357ds_gg_function_1((int64_t)(uintptr_t) pkt, 1);

    for (int64_t s = 0; s < n_seq_max; ++s) unq[s] = -1;
    *out_ids_n = 0;
    const int64_t total = cnt[0];
    for (int64_t i = 0; i < total; ++i) {
        const int64_t kind = jr[i*3], a = jr[i*3+1], b = jr[i*3+2];
        switch (kind) {
            case 0: sizes[a] = (int32_t) b; break;
            case 1: row_nseqid[a] = (int32_t) b; break;
            case 2: out_ids[(*out_ids_n)++] = (int32_t) b; break;
            case 3: row_output[a] = (int32_t) b; break;
            case 4: unq[a] = (int32_t) b; break;
            default: break;
        }
    }
    free(pkt); free(cnt); free(jr); free(sq); free(ns); free(lg); free(ix);
    return 0;
}

int fg357s_enabled(void) {
    const char * f = getenv("GGML_GOOSE_UBATCH_ADD");
    return !(f && strcmp(f, "0") == 0);
}
