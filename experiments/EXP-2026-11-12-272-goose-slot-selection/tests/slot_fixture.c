// Differential gate for the server slot-selection decisions (P8-03).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fg272s_fail      gate_fail
#define fg272s_ld_i64    gate_ld
#define fg272s_st_i64    gate_st
#define fg272s_from_bits gate_from_bits
#include "slot_selection-functions.c"

void gate_fail(int64_t c) { fprintf(stderr, "FG272S validation code %lld\n", (long long) c); abort(); }
int64_t gate_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
void gate_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }
float gate_from_bits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

static uint32_t bits_of(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

static int64_t ref_similar(const int64_t * ok, const int64_t * lcp, int64_t n, int64_t n_tok, float thr) {
    float best = 0; int64_t ret = -1;
    for (int64_t i = 0; i < n; ++i) {
        if (!ok[i]) continue;
        const float sim = (float) lcp[i] / (float) n_tok;
        if (sim > best && sim > thr) { best = sim; ret = i; }
    }
    return ret;
}

static int64_t ref_lru(const int64_t * idle, const int64_t * empty, const int64_t * tl, int64_t n) {
    int64_t ret = -1;
    for (int64_t i = 0; i < n; ++i) {
        if (!idle[i]) continue;
        if (ret < 0) { ret = i; continue; }
        if ((empty[ret] == 0 && empty[i] != 0) || (empty[ret] == empty[i] && tl[i] < tl[ret])) ret = i;
    }
    return ret;
}

static int failures, checks;
static void expect(const char * nm, int64_t got, int64_t want) {
    checks++;
    if (got != want) { failures++; printf("%-30s got %lld want %lld\n", nm, (long long) got, (long long) want); }
    else printf("%-30s OK\n", nm);
}

int main(void) {
    const int64_t ok[4]    = { 1, 1, 1, 0 };
    const int64_t lcp[4]   = { 10, 40, 25, 99 };
    expect("similar best", fg272s_pick_similar((uint64_t)(uintptr_t) ok, (uint64_t)(uintptr_t) lcp, 4, 100, bits_of(0.5f)),
           ref_similar(ok, lcp, 4, 100, 0.5f));
    expect("similar below thr", fg272s_pick_similar((uint64_t)(uintptr_t) ok, (uint64_t)(uintptr_t) lcp, 4, 100, bits_of(0.6f)),
           ref_similar(ok, lcp, 4, 100, 0.6f));
    {
        const int64_t lcp_t[4] = { 60, 60, 30, 0 };
        expect("similar tie keeps first", fg272s_pick_similar((uint64_t)(uintptr_t) ok, (uint64_t)(uintptr_t) lcp_t, 4, 100, bits_of(0.5f)),
               ref_similar(ok, lcp_t, 4, 100, 0.5f));
    }
    expect("similar none eligible", fg272s_pick_similar((uint64_t)(uintptr_t) ok, (uint64_t)(uintptr_t) lcp, 0, 100, bits_of(0.1f)),
           ref_similar(ok, lcp, 0, 100, 0.1f));
    {
        const int64_t idle[4]  = { 1, 0, 1, 1 };
        const int64_t empty[4] = { 0, 0, 1, 0 };
        const int64_t tl[4]    = { 50, 10, 90, 5 };
        expect("lru prefers empty", fg272s_pick_lru((uint64_t)(uintptr_t) idle, (uint64_t)(uintptr_t) empty, (uint64_t)(uintptr_t) tl, 4),
               ref_lru(idle, empty, tl, 4));
        const int64_t empty2[4] = { 0, 0, 0, 0 };
        expect("lru oldest", fg272s_pick_lru((uint64_t)(uintptr_t) idle, (uint64_t)(uintptr_t) empty2, (uint64_t)(uintptr_t) tl, 4),
               ref_lru(idle, empty2, tl, 4));
        const int64_t tl2[4] = { 7, 7, 7, 7 };
        expect("lru tie keeps first", fg272s_pick_lru((uint64_t)(uintptr_t) idle, (uint64_t)(uintptr_t) empty2, (uint64_t)(uintptr_t) tl2, 4),
               ref_lru(idle, empty2, tl2, 4));
        const int64_t idle_none[4] = { 0, 0, 0, 0 };
        expect("lru none idle", fg272s_pick_lru((uint64_t)(uintptr_t) idle_none, (uint64_t)(uintptr_t) empty2, (uint64_t)(uintptr_t) tl, 4),
               ref_lru(idle_none, empty2, tl, 4));
    }
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
