// Differential gate for the batch admissibility checks (P6-07).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fg291b_fail   gate_fail
#define fg291b_ld_i64 gate_ld
#define fg291b_ld_u64 gate_ld
#define fg291b_st_i64 gate_st
#include "batch_reject-functions.c"

void gate_fail(int64_t c) { fprintf(stderr, "FG291B validation code %lld\n", (long long) c); abort(); }
int64_t gate_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
void gate_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

static int64_t ref_cont(int64_t n_tok, const int64_t * pos, const int64_t * mn, const int64_t * mx,
                        const int64_t * cnt, int64_t has_mem, const int64_t * mem_max, int64_t n_seq) {
    (void) n_tok; (void) pos;
    if (!has_mem) return 0;
    for (int64_t s = 0; s < n_seq; ++s) {
        if (cnt[s] > 0) {
            const int64_t p0 = mem_max[s];
            if (p0 >= 0) {
                if (mn[s] != p0 + 1) return 1;
                if (mx[s] - mn[s] + 1 > cnt[s]) return 1;
            }
        }
    }
    return 0;
}
static int64_t ref_dec(int64_t n_tok, const int64_t * pos, const int64_t * seq_of, int64_t n_seq) {
    int64_t cur[64];
    for (int64_t s = 0; s < n_seq; ++s) cur[s] = -1;
    for (int64_t i = 0; i < n_tok; ++i) {
        const int64_t sid = seq_of[i];
        if (sid >= 0) {
            if (pos[i] < cur[sid]) return 2;
            cur[sid] = pos[i];
        }
    }
    return 0;
}
static int64_t ref_sets(int64_t n_tok, const int64_t * mask, int64_t n_seq) {
    int64_t cur[64];
    for (int64_t s = 0; s < n_seq; ++s) cur[s] = (int64_t) 0xFFFFFFFFFFFFFFFFull;
    for (int64_t i = 0; i < n_tok; ++i) {
        for (int64_t s = 0; s < n_seq; ++s) {
            if ((mask[i] >> s) & 1) {
                cur[s] &= mask[i];
                if (cur[s] == 0) return 3;
            }
        }
    }
    return 0;
}

static int failures, checks;
static void expect(const char * nm, int64_t got, int64_t want) {
    checks++;
    if (got != want) { failures++; printf("%-34s got %lld want %lld\n", nm, (long long) got, (long long) want); }
    else printf("%-34s OK\n", nm);
}

int main(void) {
    int64_t scratch[64];
    {
        const int64_t pos[3] = { 6, 7, 8 }, mn[2] = { 6, 0 }, mx[2] = { 8, 0 }, cnt[2] = { 3, 0 }, mem[2] = { 5, -1 };
        expect("cont: valid continuation", fg291b_check_continuity(3, (uint64_t)(uintptr_t)pos, (uint64_t)(uintptr_t)mn,
            (uint64_t)(uintptr_t)mx, (uint64_t)(uintptr_t)cnt, 1, (uint64_t)(uintptr_t)mem, 2),
            ref_cont(3, pos, mn, mx, cnt, 1, mem, 2));
        const int64_t mn_bad[2] = { 5, 0 };
        expect("cont: gap rejected", fg291b_check_continuity(3, (uint64_t)(uintptr_t)pos, (uint64_t)(uintptr_t)mn_bad,
            (uint64_t)(uintptr_t)mx, (uint64_t)(uintptr_t)cnt, 1, (uint64_t)(uintptr_t)mem, 2),
            ref_cont(3, pos, mn_bad, mx, cnt, 1, mem, 2));
        const int64_t mx_span[2] = { 9, 0 }, cnt_small[2] = { 2, 0 };
        expect("cont: span over count", fg291b_check_continuity(3, (uint64_t)(uintptr_t)pos, (uint64_t)(uintptr_t)mn,
            (uint64_t)(uintptr_t)mx_span, (uint64_t)(uintptr_t)cnt_small, 1, (uint64_t)(uintptr_t)mem, 2),
            ref_cont(3, pos, mn, mx_span, cnt_small, 1, mem, 2));
        expect("cont: no memory", fg291b_check_continuity(3, (uint64_t)(uintptr_t)pos, (uint64_t)(uintptr_t)mn_bad,
            (uint64_t)(uintptr_t)mx, (uint64_t)(uintptr_t)cnt, 0, (uint64_t)(uintptr_t)mem, 2),
            ref_cont(3, pos, mn_bad, mx, cnt, 0, mem, 2));
    }
    {
        const int64_t pos_ok[4] = { 1, 2, 3, 4 }, seq_ok[4] = { 0, 0, 0, 0 };
        const int64_t seq_two[4] = { 0, 1, 0, 1 };
        const int64_t pos_bad[4] = { 1, 3, 2, 4 };
        expect("dec: increasing", fg291b_check_decreasing(4, (uint64_t)(uintptr_t)pos_ok, (uint64_t)(uintptr_t)seq_ok, 2, (uint64_t)(uintptr_t)scratch),
            ref_dec(4, pos_ok, seq_ok, 2));
        expect("dec: interleaved seqs", fg291b_check_decreasing(4, (uint64_t)(uintptr_t)pos_ok, (uint64_t)(uintptr_t)seq_two, 2, (uint64_t)(uintptr_t)scratch),
            ref_dec(4, pos_ok, seq_two, 2));
        expect("dec: decreasing rejected", fg291b_check_decreasing(4, (uint64_t)(uintptr_t)pos_bad, (uint64_t)(uintptr_t)seq_ok, 2, (uint64_t)(uintptr_t)scratch),
            ref_dec(4, pos_bad, seq_ok, 2));
    }
    {
        const int64_t m_ok[3] = { 0b11, 0b11, 0b01 };
        const int64_t m_bad[3] = { 0b01, 0b10, 0b01 };
        expect("sets: compatible", fg291b_check_seq_sets(3, (uint64_t)(uintptr_t)m_ok, 2, (uint64_t)(uintptr_t)scratch),
            ref_sets(3, m_ok, 2));
        expect("sets: incompatible rejected", fg291b_check_seq_sets(3, (uint64_t)(uintptr_t)m_bad, 2, (uint64_t)(uintptr_t)scratch),
            ref_sets(3, m_bad, 2));
    }
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
