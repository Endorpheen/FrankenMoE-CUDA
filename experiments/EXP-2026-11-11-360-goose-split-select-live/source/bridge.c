// Bridge for the split_equal sequence-set selection (EXP350) into the accepted line.
//
// The module receives the used flags, the per-row set masks and first sequence ids, the ubatch limit
// and the map from mask to row indices; it returns the accepted row indices and the per-set cursors.
// llama rebuilds cur_seq_set from seq_set[row].
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;
static void fg350s_tick(void){
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG350S_SPLIT_SELECT first call calls=1\n");
    if ((n % 100) == 0) fprintf(stderr, "FG350S_SPLIT_SELECT calls=%llu\n", n);
}
static void g_fail(int64_t c){ fprintf(stderr, "FG350S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p, int64_t k, int64_t i, uint64_t v){ ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t) v; }
static uint64_t g_get(int64_t p, int64_t k, int64_t i){ return (uint64_t) ((const int64_t *)(uintptr_t)p)[k*16 + i]; }
static void   g_stw(uint64_t b, int64_t o, int64_t v){ *(int64_t *)(uintptr_t)(b + (uint64_t) o) = v; }
static int64_t g_ld(uint64_t b, int64_t o){ return *(const int64_t *)(uintptr_t)(b + (uint64_t) o); }

#define fg350k_get   g_get
#define fg350k_put   g_put
#define fg350k_ld_i64 g_ld
#define fg350k_stw   g_stw
#define fg350k_fail  g_fail
#define gg_init_globals fg350s_init_globals
#define main fg350s_generated_main
#include "host-qualified.c"
#undef main

// returns n_seqs (0 when nothing was chosen); fills accepted rows and their cursors
int64_t fg350s_select(int64_t n_tokens, const int32_t * used, const uint64_t * seq_set,
                      const int32_t * seq_first, int64_t n_ubatch, const int32_t * set_map,
                      const int32_t * set_off, int64_t n_sets, int64_t sequential,
                      int32_t * out_rows, int32_t * out_rows_n, int32_t * out_cur) {
    fg350s_tick();
    const int64_t n = n_tokens > 0 ? n_tokens : 1;
    int64_t * us = (int64_t *) malloc(sizeof(int64_t) * (size_t) n);
    int64_t * ss = (int64_t *) malloc(sizeof(int64_t) * (size_t) n);
    int64_t * sf = (int64_t *) malloc(sizeof(int64_t) * (size_t) n);
    int64_t * sm = (int64_t *) malloc(sizeof(int64_t) * (size_t)(set_off[n_sets] > 0 ? set_off[n_sets] : 1));
    int64_t * so = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n_sets + 1));
    int64_t * ch = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n_ubatch + 2));
    if (!us || !ss || !sf || !sm || !so || !ch) abort();
    for (int64_t i = 0; i < n_tokens; ++i) {
        us[i] = used ? (used[i] ? 1 : 0) : 0;
        ss[i] = seq_set ? (int64_t) seq_set[i] : 0;
        sf[i] = seq_first ? seq_first[i] : 0;
    }
    for (int64_t i = 0; i < set_off[n_sets]; ++i) sm[i] = set_map ? set_map[i] : 0;
    for (int64_t s = 0; s <= n_sets; ++s) so[s] = set_off[s];

    const int64_t cap = 4*n_tokens + 4*n_sets + 32;
    int64_t * jr = (int64_t *) malloc(sizeof(int64_t) * 3 * (size_t) cap);
    int64_t * cnt = (int64_t *) calloc(4, sizeof(int64_t));
    int64_t * pkt = (int64_t *) calloc(8*16, sizeof(int64_t));
    if (!jr || !cnt || !pkt) abort();
    pkt[0*16 + 0] = n_tokens; pkt[0*16 + 1] = (int64_t)(uintptr_t) us;
    pkt[0*16 + 2] = (int64_t)(uintptr_t) ss; pkt[0*16 + 3] = (int64_t)(uintptr_t) sf;
    pkt[0*16 + 4] = n_ubatch; pkt[0*16 + 5] = (int64_t)(uintptr_t) sm;
    pkt[0*16 + 6] = (int64_t)(uintptr_t) so; pkt[0*16 + 7] = n_sets;
    pkt[1*16 + 0] = (int64_t)(uintptr_t) jr; pkt[1*16 + 1] = (int64_t)(uintptr_t) cnt;
    pkt[1*16 + 2] = sequential; pkt[1*16 + 3] = (int64_t)(uintptr_t) ch; pkt[1*16 + 4] = n_ubatch + 1;

    (void) fg350ds_gg_function_1((int64_t)(uintptr_t) pkt, 1);

    int64_t n_seqs = 0, n_cur = 0;
    *out_rows_n = 0;
    const int64_t total = cnt[0];
    for (int64_t i = 0; i < total; ++i) {
        const int64_t kind = jr[i*3], a = jr[i*3+1], b = jr[i*3+2];
        if (kind == 0) { out_rows[(*out_rows_n)++] = (int32_t) a; }
        else if (kind == 4) { if (out_cur) out_cur[n_cur++] = (int32_t) b; }
        else if (kind == 5) { n_seqs = a; }
    }
    (void) n_cur;
    free(pkt); free(cnt); free(jr); free(ch); free(so); free(sm); free(sf); free(ss); free(us);
    return n_seqs;
}

int fg350s_enabled(void) {
    const char * f = getenv("GGML_GOOSE_SPLIT_SELECT");
    return !(f && strcmp(f, "0") == 0);
}
