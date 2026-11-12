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
static int64_t ref_walk(int64_t n_tok, const int64_t * pos, const int64_t * mask, int64_t n_seq) {
    int64_t cur_set[64], cur_pos[64];
    for (int64_t s = 0; s < n_seq; ++s) { cur_set[s] = (int64_t) 0xFFFFFFFFFFFFFFFFull; cur_pos[s] = -1; }
    for (int64_t i = 0; i < n_tok; ++i) {
        for (int64_t s = 0; s < n_seq; ++s) {
            if ((mask[i] >> s) & 1) {
                cur_set[s] &= mask[i];
                if (cur_set[s] == 0) return 3;
                if (pos[i] < cur_pos[s]) return 2;
                cur_pos[s] = pos[i];
            }
        }
    }
    return 0;
}

static int64_t ref_mrope(int64_t n_tok, const int64_t * mask, const int64_t * embd, int64_t mixed, int64_t has_token,
                         int64_t n_seq, const int64_t * mn, const int64_t * cnt, int64_t has_mem, const int64_t * mem) {
    int64_t first[64], seen[64];
    const int64_t def = has_token ? 0 : 1;
    for (int64_t s = 0; s < n_seq; ++s) { first[s] = def; seen[s] = 0; }
    if (mixed) {
        for (int64_t i = 0; i < n_tok; ++i)
            for (int64_t k = 0; k < n_seq; ++k)
                if (((mask[i] >> k) & 1) && !seen[k]) { seen[k] = 1; first[k] = embd[i]; }
    }
    if (!has_mem) return 0;
    for (int64_t s = 0; s < n_seq; ++s) {
        if (cnt[s] > 0) {
            const int64_t p0 = mem[s];
            if (p0 >= 0) {
                if (!first[s]) { if (p0 >= mn[s]) return 1; }
                else            { if (p0 >  mn[s]) return 1; }
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
        const int64_t pos_ok[4]   = { 1, 2, 3, 4 }, m_ok[4]   = { 0b1, 0b1, 0b1, 0b1 };
        const int64_t m_multi[4]  = { 0b11, 0b11, 0b01, 0b01 }, pos_multi[4] = { 1, 2, 3, 4 };
        const int64_t m_bad[3]    = { 0b01, 0b10, 0b01 };
        const int64_t pos_bad[3]  = { 5, 6, 4 }, m_dec[3] = { 0b1, 0b1, 0b1 };
        const int64_t pos_dec[3]  = { 5, 7, 6 };
        expect("walk: increasing", fg291b_check_walk(4, (uint64_t)(uintptr_t)pos_ok, (uint64_t)(uintptr_t)m_ok, 2, (uint64_t)(uintptr_t)scratch, (uint64_t)(uintptr_t)(scratch+32)),
            ref_walk(4, pos_ok, m_ok, 2));
        expect("walk: multi-seq", fg291b_check_walk(4, (uint64_t)(uintptr_t)pos_multi, (uint64_t)(uintptr_t)m_multi, 2, (uint64_t)(uintptr_t)scratch, (uint64_t)(uintptr_t)(scratch+32)),
            ref_walk(4, pos_multi, m_multi, 2));
        expect("walk: incompatible sets", fg291b_check_walk(3, (uint64_t)(uintptr_t)pos_bad, (uint64_t)(uintptr_t)m_bad, 2, (uint64_t)(uintptr_t)scratch, (uint64_t)(uintptr_t)(scratch+32)),
            ref_walk(3, pos_bad, m_bad, 2));
        expect("walk: decreasing rejected", fg291b_check_walk(3, (uint64_t)(uintptr_t)pos_dec, (uint64_t)(uintptr_t)m_dec, 2, (uint64_t)(uintptr_t)scratch, (uint64_t)(uintptr_t)(scratch+32)),
            ref_walk(3, pos_dec, m_dec, 2));
    }
    {
        int64_t sc1[64], sc2[64];
        const int64_t m1[1] = { 0b1 }, e0[1] = { 0 }, e1[1] = { 1 };
        const int64_t mn6[2] = { 6, 0 }, mn5[2] = { 5, 0 }, mn4[2] = { 4, 0 };
        const int64_t cnt[2] = { 1, 0 }, mem5[2] = { 5, -1 };
        expect("mrope: token strict ok", fg291b_check_mrope(1, (uint64_t)(uintptr_t)m1, (uint64_t)(uintptr_t)e0, 0, 1, 2,
                (uint64_t)(uintptr_t)mn6, (uint64_t)(uintptr_t)cnt, 1, (uint64_t)(uintptr_t)mem5, (uint64_t)(uintptr_t)sc1, (uint64_t)(uintptr_t)sc2),
                ref_mrope(1, m1, e0, 0, 1, 2, mn6, cnt, 1, mem5));
        expect("mrope: token equality rejected", fg291b_check_mrope(1, (uint64_t)(uintptr_t)m1, (uint64_t)(uintptr_t)e0, 0, 1, 2,
                (uint64_t)(uintptr_t)mn5, (uint64_t)(uintptr_t)cnt, 1, (uint64_t)(uintptr_t)mem5, (uint64_t)(uintptr_t)sc1, (uint64_t)(uintptr_t)sc2),
                ref_mrope(1, m1, e0, 0, 1, 2, mn5, cnt, 1, mem5));
        expect("mrope: embedding equality allowed", fg291b_check_mrope(1, (uint64_t)(uintptr_t)m1, (uint64_t)(uintptr_t)e1, 0, 0, 2,
                (uint64_t)(uintptr_t)mn5, (uint64_t)(uintptr_t)cnt, 1, (uint64_t)(uintptr_t)mem5, (uint64_t)(uintptr_t)sc1, (uint64_t)(uintptr_t)sc2),
                ref_mrope(1, m1, e1, 0, 0, 2, mn5, cnt, 1, mem5));
        expect("mrope: embedding behind rejected", fg291b_check_mrope(1, (uint64_t)(uintptr_t)m1, (uint64_t)(uintptr_t)e1, 0, 0, 2,
                (uint64_t)(uintptr_t)mn4, (uint64_t)(uintptr_t)cnt, 1, (uint64_t)(uintptr_t)mem5, (uint64_t)(uintptr_t)sc1, (uint64_t)(uintptr_t)sc2),
                ref_mrope(1, m1, e1, 0, 0, 2, mn4, cnt, 1, mem5));
        {
            const int64_t m2[2] = { 0b1, 0b1 }, first_token[2] = { 0, 1 }, first_embd[2] = { 1, 0 };
            expect("mrope: mixed first token wins", fg291b_check_mrope(2, (uint64_t)(uintptr_t)m2, (uint64_t)(uintptr_t)first_token, 1, 1, 2,
                    (uint64_t)(uintptr_t)mn5, (uint64_t)(uintptr_t)cnt, 1, (uint64_t)(uintptr_t)mem5, (uint64_t)(uintptr_t)sc1, (uint64_t)(uintptr_t)sc2),
                    ref_mrope(2, m2, first_token, 1, 1, 2, mn5, cnt, 1, mem5));
            expect("mrope: mixed first embedding wins", fg291b_check_mrope(2, (uint64_t)(uintptr_t)m2, (uint64_t)(uintptr_t)first_embd, 1, 1, 2,
                    (uint64_t)(uintptr_t)mn5, (uint64_t)(uintptr_t)cnt, 1, (uint64_t)(uintptr_t)mem5, (uint64_t)(uintptr_t)sc1, (uint64_t)(uintptr_t)sc2),
                    ref_mrope(2, m2, first_embd, 1, 1, 2, mn5, cnt, 1, mem5));
        }
        expect("mrope: no memory", fg291b_check_mrope(1, (uint64_t)(uintptr_t)m1, (uint64_t)(uintptr_t)e0, 0, 1, 2,
                (uint64_t)(uintptr_t)mn5, (uint64_t)(uintptr_t)cnt, 0, (uint64_t)(uintptr_t)mem5, (uint64_t)(uintptr_t)sc1, (uint64_t)(uintptr_t)sc2),
                ref_mrope(1, m1, e0, 0, 1, 2, mn5, cnt, 0, mem5));
    }
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
