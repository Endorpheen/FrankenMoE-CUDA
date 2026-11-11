// Bridge for the KV state-read header decisions (EXP365).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;
static void fg365s_tick(void){
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG365S_KV_READ_DATA first call\n");
    if ((n % 100) == 0) fprintf(stderr, "FG365S_KV_READ_DATA calls=%llu\n", n);
}
static void g_fail(int64_t c){ fprintf(stderr, "FG365S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p, int64_t k, int64_t i, uint64_t v){ ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t) v; }
static uint64_t g_get(int64_t p, int64_t k, int64_t i){ return (uint64_t) ((const int64_t *)(uintptr_t)p)[k*16 + i]; }
static void   g_stw(uint64_t b, int64_t o, int64_t v){ *(int64_t *)(uintptr_t)(b + (uint64_t) o) = v; }
static int64_t g_ld(uint64_t b, int64_t o){ return *(const int64_t *)(uintptr_t)(b + (uint64_t) o); }

#define fg365k_get   g_get
#define fg365k_put   g_put
#define fg365k_ld_i64 g_ld
#define fg365k_stw   g_stw
#define fg365k_fail  g_fail
#define gg_init_globals fg365s_init_globals
#define main fg365s_generated_main
#include "host-qualified.c"
#undef main

// returns the first rejection code (1..8) or 0; masks say which tensors to transfer
int64_t fg365s_read_data(int64_t nl_ref, int64_t nl, int64_t cc, int64_t size, int64_t s_trans,
                         const int32_t * rp, const int32_t * pp, const int32_t * sp,
                         int64_t rt, int64_t rr, int64_t pr, int64_t st, int64_t sr,
                         uint64_t * k_mask, uint64_t * p_mask, uint64_t * v_mask) {
    fg365s_tick();
    const int64_t n = nl > 0 ? nl : 1;
    int64_t * r64 = (int64_t *) malloc(sizeof(int64_t) * (size_t) n);
    int64_t * p64 = (int64_t *) malloc(sizeof(int64_t) * (size_t) n);
    int64_t * s64 = (int64_t *) malloc(sizeof(int64_t) * (size_t) n);
    if (!r64 || !p64 || !s64) abort();
    for (int64_t i = 0; i < nl; ++i) {
        r64[i] = rp ? (rp[i] ? 1 : 0) : 0;
        p64[i] = pp ? (pp[i] ? 1 : 0) : 0;
        s64[i] = sp ? (sp[i] ? 1 : 0) : 0;
    }
    const int64_t cap = nl + 8;
    int64_t * jr = (int64_t *) malloc(sizeof(int64_t) * 3 * (size_t) cap);
    int64_t * cnt = (int64_t *) calloc(4, sizeof(int64_t));
    int64_t * pkt = (int64_t *) calloc(8*16, sizeof(int64_t));
    if (!jr || !cnt || !pkt) abort();
    pkt[0*16 + 0] = nl_ref; pkt[0*16 + 1] = nl; pkt[0*16 + 2] = cc; pkt[0*16 + 3] = size;
    pkt[0*16 + 4] = s_trans;
    pkt[0*16 + 5] = (int64_t)(uintptr_t) r64; pkt[0*16 + 6] = (int64_t)(uintptr_t) p64;
    pkt[0*16 + 7] = (int64_t)(uintptr_t) s64;
    pkt[0*16 + 8] = rt; pkt[0*16 + 9] = rr; pkt[0*16 + 10] = pr;
    pkt[0*16 + 11] = st; pkt[0*16 + 12] = sr;
    pkt[1*16 + 0] = (int64_t)(uintptr_t) jr; pkt[1*16 + 1] = (int64_t)(uintptr_t) cnt;

    (void) fg365ds_gg_function_1((int64_t)(uintptr_t) pkt, 1);

    *k_mask = 0; *p_mask = 0; *v_mask = 0;
    int64_t code = 0;
    const int64_t total = cnt[0];
    for (int64_t i = 0; i < total; ++i) {
        const int64_t kind = jr[i*3], a = jr[i*3+1], b = jr[i*3+2];
        if (kind == 0) {
            if (a == 0) *k_mask = (uint64_t) b;
            else if (a == 1) *p_mask = (uint64_t) b;
            else if (a == 2) *v_mask = (uint64_t) b;
        }
    }
    code = (int64_t) pkt[2*16 + 1];
    free(pkt); free(cnt); free(jr); free(s64); free(p64); free(r64);
    return code < 0 ? -code : 0;
}

int fg365s_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_READ_DATA");
    return !(f && strcmp(f, "0") == 0);
}
