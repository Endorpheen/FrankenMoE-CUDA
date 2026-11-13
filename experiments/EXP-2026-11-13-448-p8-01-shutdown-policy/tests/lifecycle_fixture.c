// Differential fixture for the Goose shutdown policy (EXP448).
//
// Right arm: a C transcription of server.cpp signal_handler, both
// shutdown_handler bodies, both clean_up bodies and the exit-code returns.
// Left arm: the Goose module emitted by the pinned FrankenGoose compiler.
#include <stdio.h>
#include "ffi-adapter.h"
#include <stdint.h>

void fg448_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

enum {
    STEP_SESSION_GC = 1, STEP_STOPPING = 2, STEP_UNLOAD_ALL = 3,
    STEP_MCP = 4, STEP_BACKEND_FREE = 5, STEP_HTTP_STOP = 6, STEP_CTX_TERMINATE = 7,
};

// signal_handler: the atomic flag forces exit(1) on the second interrupt.
static void native_on_signal(int is_terminating, int has_handler, int64_t out[3]) {
    if (is_terminating) { out[0] = 2; out[1] = 1; out[2] = 1; return; }
    if (has_handler) { out[0] = 1; out[1] = 0; out[2] = 0; return; }
    out[0] = 0; out[1] = 0; out[2] = 0;
}

// shutdown_handler (kind 0) and clean_up (kind 1) per mode (0 router, 1 single).
static void native_plan(int kind, int mode, int64_t out[6]) {
    const int64_t router_signal[]  = {STEP_STOPPING, STEP_MCP, STEP_HTTP_STOP};
    const int64_t single_signal[]  = {STEP_MCP, STEP_CTX_TERMINATE};
    const int64_t router_cleanup[] = {STEP_SESSION_GC, STEP_STOPPING, STEP_UNLOAD_ALL, STEP_MCP, STEP_BACKEND_FREE};
    const int64_t single_cleanup[] = {STEP_SESSION_GC, STEP_HTTP_STOP, STEP_CTX_TERMINATE, STEP_MCP, STEP_BACKEND_FREE};
    const int64_t * src;
    int n;
    if (kind == 0 && mode == 0) { src = router_signal; n = 3; }
    else if (kind == 0) { src = single_signal; n = 2; }
    else if (mode == 0) { src = router_cleanup; n = 5; }
    else { src = single_cleanup; n = 5; }
    out[0] = n;
    for (int i = 0; i < n; ++i) out[1 + i] = src[i];
    for (int i = n; i < 5; ++i) out[1 + i] = 0;
}

static void native_exit(int outcome, int64_t out[1]) {
    out[0] = outcome ? 1 : 0;
}

static int cases = 0, checks = 0, fails = 0;

static void cmp(const char * what, int idx, const int64_t * ref, const int64_t * go, int n) {
    ++cases;
    for (int i = 0; i < n; ++i) {
        ++checks;
        if (ref[i] != go[i]) {
            ++fails;
            if (fails <= 8) printf("FAIL %s case=%d word=%d native=%lld goose=%lld\n",
                what, idx, i, (long long) ref[i], (long long) go[i]);
        }
    }
}

// Property check on a plan: ids inside 1..7, no duplicates, expected length.
static int plan_is_sane(const int64_t * p, int expect_n) {
    if (p[0] != expect_n) return 0;
    int seen[8] = {0};
    for (int i = 1; i <= expect_n; ++i) {
        if (p[i] < 1 || p[i] > 7) return 0;
        if (seen[p[i]]) return 0;
        seen[p[i]] = 1;
    }
    return 1;
}

int main(void) {
    goose_init(0, 0);
    for (int term = 0; term < 2; ++term)
    for (int handler = 0; handler < 2; ++handler)
    for (int kind = 0; kind < 2; ++kind)
    for (int mode = 0; mode < 2; ++mode)
    for (int outcome = 0; outcome < 5; ++outcome) {
        int64_t ref3[3] = {0}, go3[3] = {0};
        native_on_signal(term, handler, ref3);
        fg448_on_signal(term, handler, (uint64_t) (uintptr_t) go3);
        cmp("signal", term * 1000 + handler * 100 + kind * 10 + mode, ref3, go3, 3);

        int64_t ref6[6] = {0}, go6[6] = {0};
        native_plan(kind, mode, ref6);
        fg448_shutdown_plan(kind, mode, (uint64_t) (uintptr_t) go6);
        cmp("plan", term * 1000 + handler * 100 + kind * 10 + mode, ref6, go6, 6);

        // the plan must also be a sane, duplicate-free sequence of known steps
        ++cases;
        ++checks;
        if (!plan_is_sane(go6, (int) ref6[0])) {
            ++fails;
            printf("FAIL plan sanity kind=%d mode=%d n=%lld\n", kind, mode, (long long) go6[0]);
        }

        int64_t ref1[1] = {0}, go1[1] = {0};
        native_exit(outcome, ref1);
        fg448_exit_code(outcome, (uint64_t) (uintptr_t) go1);
        cmp("exit", outcome, ref1, go1, 1);
    }

    // Repeated-signal state machine: the second interrupt must force exit(1)
    // even when a handler is installed, and later interrupts behave the same.
    for (int handler = 0; handler < 2; ++handler)
    for (int count = 1; count <= 4; ++count) {
        int flag = 0;
        for (int s = 0; s < count; ++s) {
            int64_t ref3[3] = {0}, go3[3] = {0};
            native_on_signal(flag, handler, ref3);
            fg448_on_signal(flag, handler, (uint64_t) (uintptr_t) go3);
            cmp("sequence", handler * 10 + s, ref3, go3, 3);
            flag = 1;                       // test_and_set() keeps the flag set
        }
    }

    printf("FG448_SHUTDOWN cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
