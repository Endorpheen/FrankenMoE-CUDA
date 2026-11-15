// M1 host gate: one start-up, two threads, no crash, no second init.
//
// The risk this closes is R-1 of the P11-01 analysis: gs_program_initialized is
// a plain global with a check-then-set, and the ten staged trees each carried
// their own once-guard, so a process that linked two of them could enter
// gs_rt_init concurrently. Here there is exactly one guard, in
// host/goose_host.c, and this gate proves it under contention.
//
// Since the seam removal the same start-up also installs the IQ table
// descriptor, so the gate calls the explicit embedding entry point from both
// threads: bind is idempotent and guarded by a compare-exchange, init is
// guarded by the one pthread_once. "One init for 512 calls" covers both.
//
// It also calls one exported decision after the threads join, so a runtime that
// was never allocated cannot pass by doing nothing.
//
// Build: cmake -B build-on -DFG_GOOSE_* ... (any flag ON); the target is
// fg-host-smoke. No model, no server, no GPU.

#include "goose-host.h"
#include "goose-iq-tables.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// cfg_validate from goose/cli/config-validate.goose: an entry point that uses
// the unit's runtime but needs no model and no GPU.
typedef struct {
    int64_t * data;
    int64_t   len;
} sl_i64_v;

typedef struct {
    double  * data;
    int64_t   len;
} sl_f64_v;

extern int64_t cfg_validate(sl_i64_v, sl_f64_v);

#define THREADS      2
#define ITERATIONS 256

static int64_t g_bind_errors = 0;

static void * hammer(void * arg) {
    (void) arg;
    for (int i = 0; i < ITERATIONS; ++i) {
        if (fg_goose_host_init_with_tables(fg_goose_iq_tables_native()) != 0) {
            __atomic_add_fetch(&g_bind_errors, 1, __ATOMIC_SEQ_CST);
        }
    }
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];
    for (int i = 0; i < THREADS; ++i) {
        if (pthread_create(&t[i], NULL, hammer, NULL) != 0) {
            printf("FG_M1_HOST FAIL pthread_create\n");
            return 1;
        }
    }
    for (int i = 0; i < THREADS; ++i) {
        pthread_join(t[i], NULL);
    }

    const int64_t runs  = fg_goose_host_init_runs();
    const int     bound = fg_goose_host_iq_tables_bound();
    const int64_t berr  = __atomic_load_n(&g_bind_errors, __ATOMIC_SEQ_CST);

    // An empty configuration: the validator has nothing to complain about.
    int64_t values_i[1] = { 0 };
    double  values_f[1] = { 0.0 };
    const sl_i64_v vi = { values_i, 0 };
    const sl_f64_v vf = { values_f, 0 };
    const int64_t rc = cfg_validate(vi, vf);

    const int ok = runs == 1 && bound == 1 && berr == 0;

    printf("FG_M1_HOST threads=%d calls=%d goose_init_runs=%lld iq_bound=%d "
           "bind_errors=%lld cfg_validate=%lld %s\n",
           THREADS, THREADS * ITERATIONS, (long long) runs, bound,
           (long long) berr, (long long) rc, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
