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
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

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


// ---- trap coverage ------------------------------------------------------------------
// A negative size cannot come from a real size_t on this target, but the invariant is
// asserted in Goose and the gate must notice if the assertion is dropped.
static void run_trap(const char * what, int expected, void (*fn)(void)) {
    int fd[2];
    if (pipe(fd) != 0) { printf("FAIL pipe %s\n", what); failures++; return; }
    pid_t pid = fork();
    if (pid == 0) { close(fd[0]); dup2(fd[1], 2); close(fd[1]); fn(); _exit(0); }
    close(fd[1]);
    char buf[256] = {0};
    ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
    close(fd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    ++checks;
    int died = WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
    int code_ok = n > 0 && strstr(buf, "validation code") && atoi(strstr(buf, "validation code") + 16) == expected;
    if (!died || !code_ok) {
        printf("FAIL trap %s: died=%d code=%d expected=%d (%s)\n", what, died,
               n > 0 && strstr(buf, "validation code") ? atoi(strstr(buf, "validation code") + 16) : -1,
               expected, buf);
        ++failures;
    }
}
static void trap_alloc_neg(void)   { int64_t o[4] = {0}; fg196p_plan_host_alloc(0, -1, (uint64_t)(uintptr_t)o); }
static void trap_reg_neg(void)     { int64_t o[4] = {0}; fg196p_plan_register(0, -1, (uint64_t)(uintptr_t)o); }
static void trap_alloc_neg2(void)  { int64_t o[4] = {0}; fg196p_plan_host_alloc(1, -1024, (uint64_t)(uintptr_t)o); }

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

    run_trap("host_alloc negative size", 21, trap_alloc_neg);
    run_trap("host_alloc negative size, disabled", 21, trap_alloc_neg2);
    run_trap("register negative size", 22, trap_reg_neg);
    printf("FG196P_FIXTURE checks=%lld failures=%lld\n", checks, failures);
    if (failures) { printf("EXP196_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP196_LOCAL_GATES_PASS\n");
    return 0;
}
