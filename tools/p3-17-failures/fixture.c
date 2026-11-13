// P3-17 failure probe: drive the Goose pinned-memory plan on boundary inputs and check that the
// *decision* is made by Goose (plan[0] == 0 means "do not allocate"), not by a native fallback.
//
// Linked against the EXP-196 bridge + generated Goose code, so the functions under test are the ones
// that execute in the real backend.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern void fg196p_plan_host_alloc(int64_t disabled, int64_t size, uint64_t out);
extern void fg196p_plan_fallback(int64_t ptr_zero, uint64_t out);

static int failures;

static void expect(const char * name, int64_t disabled, int64_t size, int expect_zero) {
    int64_t plan[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    fg196p_plan_host_alloc(disabled, size, (uint64_t) (uintptr_t) plan);
    const int zero = plan[0] == 0;
    const int ok = (zero == expect_zero);
    if (!ok) failures++;
    printf("%-28s disabled=%lld size=%-12lld -> plan[0]=%-12lld %s\n",
           name, (long long) disabled, (long long) size,
           (long long) plan[0], ok ? "OK" : "MISMATCH");
}

int main(void) {
    // Baseline: a normal request must produce a usable plan.
    expect("normal", 0, 1 << 20, 0);
    // Disabled: the module must refuse regardless of size.
    expect("disabled, 1 MiB", 1, 1 << 20, 1);
    expect("disabled, 0", 1, 0, 1);
    // Zero size: nothing to allocate, so the plan must be a refusal, not a zero-byte success.
    expect("zero size", 0, 0, 1);
    // Negative size is a caller error, not an allocation decision: host.goose enforces `ensure(size>=0,21)`
    // and aborts. Documented here instead of being driven, because aborting is the module's stated contract.
    printf("%-28s %s\n", "negative size", "N/A - module aborts by contract (ensure size>=0, code 21)");
    // Very large request: the decision must still be a plan, and the caller will see the failure
    // through the allocation result rather than silently succeeding here.
    expect("huge size", 0, (int64_t) 1 << 40, 0);

    // The fallback decision is Goose's too: it is invoked only when the pointer is zero.
    {
        int64_t out = -1;
        fg196p_plan_fallback(1, (uint64_t) (uintptr_t) &out);
        printf("fallback with null pointer    -> out=%lld %s\n", (long long) out, out == 0 || out == 1 ? "OK" : "MISMATCH");
        if (!(out == 0 || out == 1)) failures++;
    }

    printf("failures: %d\n", failures);
    return failures ? 1 : 0;
}
