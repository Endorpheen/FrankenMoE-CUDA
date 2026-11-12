// P3-17 VMM probe: boundary behaviour of the Goose VMM plan and its free accounting.
//
// Linked against the EXP-195 bridge (which includes its generated Goose code) so these are the functions
// that execute in the real backend.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern void    fg195v_plan_alloc(int64_t size, int64_t pool_used, int64_t pool_size,
                                 int64_t granularity, uint64_t out);
extern int64_t fg195v_plan_free (int64_t ptr_off, int64_t pool_used, int64_t size);

static int failures;

static void alloc_case(const char * name, int64_t size, int64_t used, int64_t pool, int64_t gran,
                       int expect_new_pool) {
    int64_t plan[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    fg195v_plan_alloc(size, used, pool, gran, (uint64_t) (uintptr_t) plan);
    const int grew = plan[2] > pool;
    const int ok = (plan[0] >= 0) && (grew == expect_new_pool);
    if (!ok) failures++;
    printf("%-30s size=%-10lld used=%-10lld pool=%-10lld -> reserve=%-10lld pool'=%-10lld %s\n",
           name, (long long) size, (long long) used, (long long) pool,
           (long long) plan[0], (long long) plan[2], ok ? "OK" : "MISMATCH");
}

int main(void) {
    const int64_t G = 2 * 1024 * 1024;      // 2 MiB granularity, as the backend uses
    const int64_t P = 64 * 1024 * 1024;     // 64 MiB pool

    // Fits in the existing pool: no growth expected.
    alloc_case("fits in pool", 1 << 20, 0, P, G, 0);
    // Does not fit: the plan must grow the pool.
    alloc_case("needs growth", 32 << 20, P - (4 << 20), P, G, 1);
    // Zero and absurd sizes are rejected by contract, not by decision: device.goose has
    // `ensure(size>0 && ... , 10)`, so these abort instead of returning a plan. Recorded, not driven.
    printf("%-30s %s\n", "zero size", "N/A - aborts by contract (ensure size>0, code 10)");
    printf("%-30s %s\n", "huge size", "N/A - subject to ensure(pool_size+reserve<=max_size, code 12)");

    // Free accounting: releasing a valid offset must return the pool usage to its previous value.
    {
        const int64_t before = P - (4 << 20);
        const int64_t after  = fg195v_plan_free(before - (4 << 20), before, 4 << 20);
        const int ok = (after == before - (4 << 20));
        if (!ok) failures++;
        printf("%-30s used %lld -> %lld (expected %lld) %s\n", "free valid offset",
               (long long) before, (long long) after, (long long) (before - (4 << 20)), ok ? "OK" : "MISMATCH");
    }
    // Freeing an unknown offset is rejected by contract: device.goose has `ensure(ptr_off==new_used, 21)`,
    // which treats a non-LIFO free as a caller bug and aborts. Recorded rather than driven.
    printf("%-30s %s\n", "free unknown offset", "N/A - aborts by contract (ensure ptr_off==new_used, code 21)");

    printf("failures: %d\n", failures);
    return failures ? 1 : 0;
}
