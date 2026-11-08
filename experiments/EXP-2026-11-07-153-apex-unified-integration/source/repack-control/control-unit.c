// Adapter for the Goose CPU_REPACK control policy/scheduler.
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int64_t fg006_rc_k(void);
void fg006_rc_chunk_gemm(int64_t,int64_t,int64_t,int64_t);
void fg006_rc_chunk_gemv(int64_t,int64_t,int64_t,int64_t);
void fg006_rc_chunk_set(int64_t);
int64_t fg006_rc_chunk_add(int64_t);
void fg006_rc_barrier(void);
void fg006_rc_quant4(int64_t, int64_t, int64_t);
void fg006_rc_quant1(int64_t, int64_t, int64_t);
void fg006_rc_rows_clear(int64_t);
int32_t fg006_rc_ids_get(int64_t, int64_t);
int64_t fg006_rc_row_count(int64_t);
void fg006_rc_row_set(int64_t, int64_t, int64_t, int64_t);
void fg006_rc_row_inc(int64_t);
int64_t fg006_rc_row_i1(int64_t, int64_t);
int64_t fg006_rc_row_i2(int64_t, int64_t);
void fg006_rc_mmid_gemv(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

static atomic_uint_fast64_t rc_calls[4], rc_values[4];
static void fg006_rc_witness(int64_t mode, int64_t value) {
    if (mode < 0 || mode >= 4 || value < 0) __builtin_trap();
    atomic_fetch_add_explicit(rc_calls + mode, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(rc_values + mode, (uint64_t) value, memory_order_relaxed);
}

#define main fg006_rc_unused_main
#include "repack-control-generated.c"
#undef main

static int rc_enabled = 1;
__attribute__((constructor)) static void rc_cfg(void) {
    const char * v = getenv("FG006_CONTROL_ROLLBACK");
    rc_enabled = !v || strcmp(v, "1") != 0;
}
int fg006_rc_enabled(void) { return rc_enabled; }

int64_t fg006_rc_entry(int64_t mode, int64_t a, int64_t b, int64_t c, int64_t d,
                       int64_t e, int64_t f, int64_t g, int64_t h) {
    return gg_function_5(mode, a, b, c, d, e, f, g, h, 0);
}
void fg006_rc_report(void) {
    for (int i = 0; i < 4; ++i) {
        uint64_t n = atomic_load(rc_calls + i);
        if (n) fprintf(stderr, "FG006_RC mode=%d calls=%llu values=%llu\n", i,
                       (unsigned long long) n, (unsigned long long) atomic_load(rc_values + i));
    }
}

__attribute__((destructor)) static void fg006_rc_finish(void) {fg006_rc_report();}
