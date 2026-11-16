#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int64_t g_ld(uint64_t b, int64_t o) { return *(const int64_t *)(uintptr_t)(b + (uint64_t)o); }
static void g_stw(uint64_t b, int64_t o, int64_t v) { *(int64_t *)(uintptr_t)(b + (uint64_t)o) = v; }
static void g_fail(int64_t c) { fprintf(stderr, "GR_FAIL code=%" PRId64 "\n", c); abort(); }
#define grk_ld_i64 g_ld
#define grk_stw g_stw
#define grk_fail g_fail
#define goose_init gr_goose_init
void gr_goose_init(int, char **);
#define main gr_generated_main
#include "gr.c"
#undef main

// Differential surface for the gate: same packet gr_run reads, built from plain arrays.
int64_t gr_check(const int64_t * present, const int64_t * want, const int64_t * have, int64_t n) {
    static int started = 0;
    int64_t pkt[3][16], out = -1;
    if (!started) { gr_goose_init(0, 0); started = 1; }
    memset(pkt, 0, sizeof pkt);
    pkt[0][0] = n;
    pkt[1][0] = (int64_t)(uintptr_t) present;
    pkt[1][1] = (int64_t)(uintptr_t) want;
    pkt[1][2] = (int64_t)(uintptr_t) have;
    pkt[2][0] = (int64_t)(uintptr_t) &out;
    return gr_run((int64_t)(uintptr_t) pkt);
}
