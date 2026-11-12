// Differential gate for the server-queue decisions (P8-03).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fg270q_fail   gate_fail
#define fg270q_ld_i64 gate_ld
#define fg270q_st_i64 gate_st
#include "queue_decisions-functions.c"

void gate_fail(int64_t c) { fprintf(stderr, "FG270Q validation code %lld\n", (long long) c); abort(); }
int64_t gate_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
void gate_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

static int failures, checks;

static int64_t ref_pick(const int64_t * slot, int64_t n, int64_t target) {
    if (n == 0) return -1;
    for (int64_t i = 0; i < n; ++i) if (slot[i] == target) return i;
    return 0;
}
static int64_t ref_cleanup(const int64_t * ids, int64_t n, int64_t target, uint64_t * mask) {
    int64_t k = 0;
    for (int64_t i = 0; i < n; ++i) if (ids[i] == target) { mask[i/64] |= (uint64_t)1 << (i%64); k++; }
    return k;
}

static void expect(const char * n, int64_t got, int64_t want) {
    checks++;
    if (got != want) { failures++; printf("%-28s got %lld want %lld\n", n, (long long) got, (long long) want); }
    else printf("%-28s OK\n", n);
}

int main(void) {
    const int64_t slots[5] = { 3, 0, 1, 0, 2 };
    expect("pick empty",            fg270q_pick_deferred((uint64_t)(uintptr_t) slots, 0, 1), ref_pick(slots, 0, 1));
    expect("pick exact slot",       fg270q_pick_deferred((uint64_t)(uintptr_t) slots, 5, 1), ref_pick(slots, 5, 1));
    expect("pick earliest match",   fg270q_pick_deferred((uint64_t)(uintptr_t) slots, 5, 0), ref_pick(slots, 5, 0));
    expect("pick no match -> first",fg270q_pick_deferred((uint64_t)(uintptr_t) slots, 5, 9), ref_pick(slots, 5, 9));
    {
        const int64_t ids[6] = { 7, 3, 7, 9, 7, 3 };
        uint64_t gm[1] = {0}, rm[1] = {0};
        const int64_t g = fg270q_cleanup_mask((uint64_t)(uintptr_t) ids, 6, 7, (uint64_t)(uintptr_t) gm);
        const int64_t r = ref_cleanup(ids, 6, 7, rm);
        expect("cleanup match 7", g, r);
        expect("cleanup mask 7", (int64_t) gm[0], (int64_t) rm[0]);
        gm[0] = rm[0] = 0;
        const int64_t g2 = fg270q_cleanup_mask((uint64_t)(uintptr_t) ids, 6, 42, (uint64_t)(uintptr_t) gm);
        expect("cleanup no match", g2, ref_cleanup(ids, 6, 42, rm));
        expect("cleanup mask none", (int64_t) gm[0], (int64_t) rm[0]);
    }
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
