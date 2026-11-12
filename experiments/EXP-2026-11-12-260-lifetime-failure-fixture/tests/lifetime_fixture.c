// Shared lifetime fixture for the two pool-policy modules of P3-17.
//
// It drives the same decision functions the CUDA backend calls, with the boundary, refusal and
// accounting cases the criterion names, and checks values -- not only "nonzero".
//
//   fg196p_*  pinned-host plan / register / unregister / fallback  (EXP-196 module)
//   fg195v_*  VMM alloc plan / free accounting                     (EXP-195 module)
//
// Refusals that the modules implement as ensure() -> abort (negative size, VMM zero size, a free
// that does not sit at the top of the pool) are documented rather than driven: aborting is their
// stated contract, and a fixture that can die is not measuring the decision.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void    fg196p_plan_host_alloc(int64_t disabled, int64_t size, uint64_t out);
extern void    fg196p_plan_host_alloc_result(int64_t err, uint64_t out);
extern void    fg196p_plan_fallback(int64_t ptr_zero, uint64_t out);
extern void    fg196p_plan_register(int64_t enabled, int64_t size, uint64_t out);
extern void    fg196p_plan_register_result(int64_t err, uint64_t out);
extern void    fg196p_plan_unregister(int64_t enabled, int64_t err, uint64_t out);
extern void    fg195v_plan_alloc(int64_t size, int64_t pool_used, int64_t pool_size,
                                 int64_t granularity, uint64_t out);
extern int64_t fg195v_plan_free(int64_t ptr_off, int64_t pool_used, int64_t size);

static int failures;
static int checks;

static void check(const char * name, const char * field, int64_t got, int64_t want) {
    checks++;
    if (got != want) {
        failures++;
        printf("%-34s %-10s got %-14lld want %-14lld MISMATCH\n",
               name, field, (long long) got, (long long) want);
    }
}

static void check_free(const char * name, int64_t got, int64_t want) {
    checks++;
    if (got != want) {
        failures++;
        printf("%-34s free       got %-14lld want %-14lld MISMATCH\n",
               name, (long long) got, (long long) want);
    }
}

static void pinned(const char * name, int64_t disabled, int64_t size, int64_t want_attempt) {
    int64_t plan[4] = { -1, -1, -1, -1 };
    fg196p_plan_host_alloc(disabled, size, (uint64_t) (uintptr_t) plan);
    check(name, "attempt", plan[0], want_attempt);
    if (size > 0) {
        // MiB in thousandths: size * 1000 / 1048576, computed without floating point.
        const int64_t want_milli = size / 1048576 * 1000 + size % 1048576 * 1000 / 1048576;
        check(name, "mib_milli", plan[1], want_milli);
    }
}

static void alloc(const char * name, int64_t size, int64_t used, int64_t pool, int64_t gran,
                  int64_t reserve, int64_t ptr_off, int64_t new_pool, int64_t new_used,
                  int64_t actual, int64_t map_off) {
    int64_t plan[6] = { -1, -1, -1, -1, -1, -1 };
    fg195v_plan_alloc(size, used, pool, gran, (uint64_t) (uintptr_t) plan);
    check(name, "reserve", plan[0], reserve);
    check(name, "ptr_off", plan[1], ptr_off);
    check(name, "pool",    plan[2], new_pool);
    check(name, "used",    plan[3], new_used);
    check(name, "actual",  plan[4], actual);
    check(name, "map_off", plan[5], map_off);
}

int main(void) {
    const int64_t MiB = 1024 * 1024;

    // ---- pinned host plan -------------------------------------------------------------------
    pinned("pinned normal 1 MiB",   0, MiB, 1);
    pinned("pinned disabled",       1, MiB, 0);
    pinned("pinned disabled, zero", 1, 0,   0);
    pinned("pinned zero size",      0, 0,   0);   // the fixed divergence: was 1, VMM refuses 0
    pinned("pinned huge 1 TiB",     0, (int64_t) 1 << 40, 1);
    {
        int64_t out[4] = { -1, -1, -1, -1 };
        fg196p_plan_host_alloc_result(0, (uint64_t) (uintptr_t) out);
        check("pinned result ok",   "ok",  out[2], 1);
        check("pinned result ok",   "log", out[3], 0);
        out[0] = out[1] = out[2] = out[3] = -1;
        fg196p_plan_host_alloc_result(2, (uint64_t) (uintptr_t) out);
        check("pinned result fail", "ok",  out[2], 0);
        check("pinned result fail", "log", out[3], 1);

        out[0] = -1;
        fg196p_plan_fallback(1, (uint64_t) (uintptr_t) out);
        check("pinned fallback null", "fallback", out[0], 1);
        out[0] = -1;
        fg196p_plan_fallback(0, (uint64_t) (uintptr_t) out);
        check("pinned fallback ptr",  "fallback", out[0], 0);

        out[0] = out[1] = -1;
        fg196p_plan_register(1, MiB, (uint64_t) (uintptr_t) out);
        check("pinned register on",  "attempt", out[0], 1);
        check("pinned register on",  "mib",     out[1], 1000);
        out[0] = -1;
        fg196p_plan_register(0, MiB, (uint64_t) (uintptr_t) out);
        check("pinned register off", "attempt", out[0], 0);

        out[0] = out[1] = out[2] = out[3] = -1;
        fg196p_plan_unregister(1, 0, (uint64_t) (uintptr_t) out);
        check("pinned unregister ok",   "call", out[0], 1);
        check("pinned unregister ok",   "ok",   out[2], 1);
        check("pinned unregister ok",   "log",  out[3], 0);
        out[0] = out[1] = out[2] = out[3] = -1;
        fg196p_plan_unregister(1, 7, (uint64_t) (uintptr_t) out);
        check("pinned unregister fail", "call", out[0], 1);
        check("pinned unregister fail", "ok",   out[2], 0);
        check("pinned unregister fail", "log",  out[3], 1);
        out[0] = out[1] = out[2] = out[3] = -1;
        fg196p_plan_unregister(0, 7, (uint64_t) (uintptr_t) out);
        check("pinned unregister off",  "call", out[0], 0);
        check("pinned unregister off",  "ok",   out[2], 1);
    }

    // ---- VMM alloc plan ---------------------------------------------------------------------
    // 100 bytes into an empty pool with a 2 MiB granularity: 128-byte alignment, one granularity
    // of growth, the mapping placed at the old pool end (0).
    alloc("vmm first 100B", 100, 0, 0, 2*MiB, 2*MiB, 0, 2*MiB, 128, 128, 0);
    // The same request with room left: no growth, the pointer advances to the current top.
    alloc("vmm second 100B", 100, 128, 2*MiB, 2*MiB, 0, 128, 2*MiB, 256, 128, 2*MiB);
    // 3 MiB into an empty pool: growth is rounded up to two granularities.
    alloc("vmm grow 3 MiB", 3*MiB, 0, 0, 2*MiB, 4*MiB, 0, 4*MiB, 3*MiB, 3*MiB, 0);
    // Exactly filling the free space must not ask for growth (aligned == avail).
    alloc("vmm exact fit", 2*MiB, 0, 2*MiB, 2*MiB, 0, 0, 2*MiB, 2*MiB, 2*MiB, 2*MiB);

    // ---- VMM free accounting ----------------------------------------------------------------
    // A bump allocator: the freed block must be the current top.
    check_free("vmm free top",       fg195v_plan_free(128, 256, 128), 128);
    check_free("vmm free whole pool", fg195v_plan_free(0, 2*MiB, 2*MiB), 0);

    printf("checks: %d, failures: %d\n", checks, failures);
    printf("%-34s %s\n", "vmm zero size",        "N/A - refused by ensure code 10 (aborts)");
    printf("%-34s %s\n", "vmm free off-top",     "N/A - refused by ensure code 21 (aborts)");
    printf("%-34s %s\n", "negative size, both",  "N/A - refused by ensure (aborts)");
    return failures ? 1 : 0;
}
