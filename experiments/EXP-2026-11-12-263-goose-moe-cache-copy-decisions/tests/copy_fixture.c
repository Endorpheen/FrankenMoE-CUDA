// Differential gate for the MoE cache upload-copy decisions.
// Left: direct transcriptions of copy_experts()/copy() from llama-moe-cache.cpp. Right: the Goose module.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fg263c_fail   gate_fail
#define fg263c_ld_i32 gate_ld32
#define fg263c_st_i64 gate_st64
#include "copy_decisions-functions.c"

void gate_fail(int64_t c) { fprintf(stderr, "FG263C validation code %lld\n", (long long) c); abort(); }
int64_t gate_ld32(uint64_t b, int64_t i) { int32_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 4); return v; }
void gate_st64(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

// --- native transcriptions -------------------------------------------------------------------

static int64_t ref_run_slot(const int32_t * slots, int64_t e, int64_t last) {
    if (slots[e] < 0) return 0;
    int64_t n = 1;
    while (e + n <= last && slots[e + n] == slots[e] + n) n++;
    return n;
}

static int64_t ref_run_fills(const int32_t * expert, const int32_t * slot, int64_t i, int64_t n_fills) {
    int64_t n = 1;
    while (i + n < n_fills && expert[i + n] == expert[i] + n && slot[i + n] == slot[i] + n) n++;
    return n;
}

static int64_t ref_stats(int64_t n_ids, int64_t n_expert_used) { return n_ids <= 8 * n_expert_used ? 0 : 1; }

static int64_t ref_fill_bytes(const int32_t * expert, const int32_t * slot, int64_t n_fills, int64_t expert_size) {
    int64_t bytes = 0;
    for (int64_t i = 0; i < n_fills;) {
        const int64_t n = ref_run_fills(expert, slot, i, n_fills);
        bytes += n * expert_size;
        i += n;
    }
    return bytes;
}

// --- cases -----------------------------------------------------------------------------------

static int failures, checks;

static void eq(const char * name, int64_t got, int64_t want) {
    checks++;
    if (got != want) { failures++; printf("%-28s got %-6lld want %-6lld MISMATCH\n", name, (long long) got, (long long) want); }
}

static void case_slot(const char * name, const int32_t * slots, int n, int64_t e, int64_t last, int64_t want) {
    (void) n;
    eq(name, fg263c_run_from_slot((uint64_t) (uintptr_t) slots, e, last), want);
    eq("  (native agrees)", ref_run_slot(slots, e, last), want);
}

static void case_fills(const char * name, const int32_t * ex, const int32_t * sl, int n, int64_t i, int64_t want) {
    eq(name, fg263c_run_from_fills((uint64_t) (uintptr_t) ex, (uint64_t) (uintptr_t) sl, i, n), want);
    eq(name, want, ref_run_fills(ex, sl, i, n));
}

int main(void) {
    {
        const int32_t slots[8] = { 4, 5, 6, 9, 10, -1, 3, 4 };
        case_slot("slot single",       slots, 8, 0, 0, 1);
        case_slot("slot run of three", slots, 8, 0, 7, 3);
        case_slot("slot stops at gap", slots, 8, 3, 7, 2);
        case_slot("slot negative",     slots, 8, 5, 7, 0);
        case_slot("slot at last",      slots, 8, 7, 7, 1);
        case_slot("slot run to last",  slots, 8, 6, 7, 2);
    }
    {
        const int32_t ex[6] = { 2, 3, 4, 9, 10, 12 };
        const int32_t sl[6] = { 0, 1, 2, 7, 9, 20 };
        case_fills("fills run of three", ex, sl, 6, 0, 3);
        case_fills("fills expert jump",  ex, sl, 6, 3, 1);
        case_fills("fills slot gap",     ex, sl, 6, 4, 1);
        case_fills("fills last",         ex, sl, 6, 5, 1);
        eq("fill_bytes 60", fg263c_fill_bytes((uint64_t) (uintptr_t) ex, (uint64_t) (uintptr_t) sl, 6, 10), 60);
        eq("fill_bytes 30 ref", ref_fill_bytes(ex, sl, 6, 10), 60);
    }
    eq("stats small at bound", fg263c_stats_class(8, 1), 0);
    eq("stats large above",    fg263c_stats_class(9, 1), 1);
    eq("stats zero",           fg263c_stats_class(0, 8), 0);
    eq("stats ref small",      ref_stats(8, 1), 0);
    eq("stats ref large",      ref_stats(65, 8), 1);

    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
