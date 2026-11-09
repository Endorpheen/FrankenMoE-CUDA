// Differential gate for the Goose pinned-memory policy module.
//
// The reference is what ggml-cuda.cu does, transcribed: the flag checks as written, and the
// MiB figure computed independently in long double rather than by the same integer division
// the Goose body uses.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void fg196p_plan_host_alloc(int64_t, int64_t, uint64_t);
void fg196p_plan_host_alloc_result(int64_t, uint64_t);
void fg196p_plan_fallback(int64_t, uint64_t);
void fg196p_plan_register(int64_t, int64_t, uint64_t);
void fg196p_plan_register_result(int64_t, uint64_t);
void fg196p_plan_unregister(int64_t, int64_t, uint64_t);

static long long checks, failures;

static int64_t * slot(uint64_t p, int i) { return (int64_t *)(uintptr_t)(p + 8 * (uint64_t)i); }

static void ck(int cond, const char * what, int64_t a, int64_t b) {
    checks++;
    if (!cond) {
        if (failures < 20) printf("FAIL %s: goose=%lld reference=%lld\n", what, (long long)a, (long long)b);
        failures++;
    }
}

// Independent reference for the MiB thousandths: floor(size * 1000 / 1048576) in long double.
static int64_t mib_milli_ref(int64_t size) {
    long double v = ((long double)size * 1000.0L) / 1048576.0L;
    return (int64_t)floorl(v);
}

int main(void) {
    int64_t sizes[] = {0, 1, 1023, 1024, 4095, 4096, 1 << 20, (1 << 20) - 1, 677719168,
                       4LL << 30, (1LL << 40) + 12345, 129 * 1024 * 1024};
    int nsizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

    for (int di = 0; di < 2; di++) {
        int64_t disabled = di;
        for (int si = 0; si < nsizes; si++) {
            int64_t size = sizes[si];
            int64_t out[4] = {0, 0, 0, 0};
            fg196p_plan_host_alloc(disabled, size, (uint64_t)(uintptr_t)out);

            int64_t attempt_ref = (disabled != 0) ? 0 : 1;
            ck(*slot((uint64_t)(uintptr_t)out, 0) == attempt_ref, "host_alloc attempt",
               *slot((uint64_t)(uintptr_t)out, 0), attempt_ref);
            ck(*slot((uint64_t)(uintptr_t)out, 1) == mib_milli_ref(size), "host_alloc mib",
               *slot((uint64_t)(uintptr_t)out, 1), mib_milli_ref(size));

            // The driver outcome is independent of whether the attempt was made: the caller
            // only consults it after attempting.
            int64_t errs[] = {0, 1, 2, 4, 35, 700, 801, 999};
            for (int ei = 0; ei < (int)(sizeof(errs) / sizeof(errs[0])); ei++) {
                int64_t e = errs[ei];
                int64_t o2[4] = {0, 0, 0, 0};
                fg196p_plan_host_alloc_result(e, (uint64_t)(uintptr_t)o2);
                int64_t ok_ref = (e == 0) ? 1 : 0;
                int64_t log_ref = (e == 0) ? 0 : 1;
                ck(*slot((uint64_t)(uintptr_t)o2, 2) == ok_ref, "host_alloc ok",
                   *slot((uint64_t)(uintptr_t)o2, 2), ok_ref);
                ck(*slot((uint64_t)(uintptr_t)o2, 3) == log_ref, "host_alloc log",
                   *slot((uint64_t)(uintptr_t)o2, 3), log_ref);

                int64_t o3[4] = {0, 0, 0, 0};
                fg196p_plan_register_result(e, (uint64_t)(uintptr_t)o3);
                ck(*slot((uint64_t)(uintptr_t)o3, 2) == ok_ref, "register ok",
                   *slot((uint64_t)(uintptr_t)o3, 2), ok_ref);
                ck(*slot((uint64_t)(uintptr_t)o3, 3) == log_ref, "register log",
                   *slot((uint64_t)(uintptr_t)o3, 3), log_ref);
            }

            int64_t o4[4] = {0, 0, 0, 0};
            fg196p_plan_fallback(1, (uint64_t)(uintptr_t)o4);
            ck(*slot((uint64_t)(uintptr_t)o4, 0) == 1, "fallback on failed pin",
               *slot((uint64_t)(uintptr_t)o4, 0), 1);
            int64_t o5[4] = {0, 0, 0, 0};
            fg196p_plan_fallback(0, (uint64_t)(uintptr_t)o5);
            ck(*slot((uint64_t)(uintptr_t)o5, 0) == 0, "no fallback on success",
               *slot((uint64_t)(uintptr_t)o5, 0), 0);
        }
    }

    for (int en = 0; en < 2; en++) {
        for (int si = 0; si < nsizes; si++) {
            int64_t size = sizes[si];
            int64_t out[4] = {0, 0, 0, 0};
            fg196p_plan_register(en, size, (uint64_t)(uintptr_t)out);
            int64_t attempt_ref = (en != 0) ? 1 : 0;
            ck(*slot((uint64_t)(uintptr_t)out, 0) == attempt_ref, "register attempt",
               *slot((uint64_t)(uintptr_t)out, 0), attempt_ref);
            ck(*slot((uint64_t)(uintptr_t)out, 1) == mib_milli_ref(size), "register mib",
               *slot((uint64_t)(uintptr_t)out, 1), mib_milli_ref(size));

            int64_t errs[] = {0, 1, 4, 35, 801};
            for (int ei = 0; ei < (int)(sizeof(errs) / sizeof(errs[0])); ei++) {
                int64_t e = errs[ei];
                int64_t o[4] = {0, 0, 0, 0};
                fg196p_plan_unregister(en, e, (uint64_t)(uintptr_t)o);
                // Native: early return when registration is off; otherwise call, clear the
                // error on failure and stay silent.
                int64_t call_ref = (en != 0) ? 1 : 0;
                int64_t ok_ref   = (en == 0 || e == 0) ? 1 : 0;
                int64_t clear_ref = (en != 0 && e != 0) ? 1 : 0;
                ck(*slot((uint64_t)(uintptr_t)o, 0) == call_ref, "unregister call",
                   *slot((uint64_t)(uintptr_t)o, 0), call_ref);
                ck(*slot((uint64_t)(uintptr_t)o, 2) == ok_ref, "unregister ok",
                   *slot((uint64_t)(uintptr_t)o, 2), ok_ref);
                ck(*slot((uint64_t)(uintptr_t)o, 3) == clear_ref, "unregister clear",
                   *slot((uint64_t)(uintptr_t)o, 3), clear_ref);
            }
        }
    }

    printf("FG196P_FIXTURE checks=%lld failures=%lld\n", checks, failures);
    if (failures) { printf("EXP196_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP196_LOCAL_GATES_PASS\n");
    return 0;
}
