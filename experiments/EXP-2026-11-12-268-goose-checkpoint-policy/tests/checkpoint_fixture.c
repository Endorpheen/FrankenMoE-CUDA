// Differential gate for the context-checkpoint eviction policy.
// Left: a direct transcription of create_checkpoint() in tools/server/server-context.cpp. Right: the Goose module.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fg268p_fail   gate_fail
#define fg268p_ld_i64 gate_ld
#define fg268p_st_i64 gate_st
#include "checkpoint_policy-functions.c"

void gate_fail(int64_t c) { fprintf(stderr, "FG268P validation code %lld\n", (long long) c); abort(); }
int64_t gate_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
void gate_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

// --- native transcriptions -------------------------------------------------------------------

static int ref_near(const int64_t * tok, const int64_t * task, int count, int64_t id_task,
                    int64_t min_step, int64_t limit, uint64_t * mask) {
    int64_t size = count;
    int64_t last = -1;
    int erased = 0;
    for (int i = 0; i < count && size + 1 >= limit; ++i) {
        if (task[i] != id_task && last >= 0 && tok[i] <= last + min_step) {
            mask[i/64] |= (uint64_t) 1 << (i % 64);
            size--;
            erased++;
        } else {
            last = tok[i];
        }
    }
    return erased;
}

static int64_t ref_make_room(int64_t count, int64_t limit) {
    int64_t n = 0, size = count;
    while (size >= limit) { n++; size--; }
    return n;
}

static int64_t ref_supersede(const int64_t * tok, int count, int64_t n_new, uint64_t * mask) {
    int64_t n = 0;
    for (int i = 0; i < count; ++i) if (tok[i] == n_new) { mask[i/64] |= (uint64_t) 1 << (i % 64); n++; }
    return n;
}

// --- harness ---------------------------------------------------------------------------------

static int failures, checks;

static void expect(const char * name, int64_t got, int64_t want, const uint64_t * gm, const uint64_t * rm, int n) {
    checks++;
    int bad = got != want;
    for (int w = 0; w < (n + 63) / 64; ++w) if (gm[w] != rm[w]) bad = 1;
    if (bad) { failures++; printf("%-30s got %lld want %lld (masks '%s')\n", name, (long long) got, (long long) want, bad && got == want ? "differ" : "-"); }
    else printf("%-30s OK\n", name);
}

static void case_near(const char * name, const int64_t * tok, const int64_t * task, int count,
                      int64_t id_task, int64_t min_step, int64_t limit) {
    uint64_t gm[4] = {0, 0, 0, 0}, rm[4] = {0, 0, 0, 0};
    const int64_t g = fg268p_near_evict((uint64_t) (uintptr_t) tok, (uint64_t) (uintptr_t) task, count,
                                        id_task, min_step, limit, (uint64_t) (uintptr_t) gm);
    const int64_t r = ref_near(tok, task, count, id_task, min_step, limit, rm);
    expect(name, g, r, gm, rm, count);
}

int main(void) {
    {
        const int64_t tok[6] = { 10, 12, 14, 16, 18, 20 };
        const int64_t task[6] = { 1, 1, 1, 1, 1, 1 };
        case_near("near: not full", tok, task, 6, 9, 2, 32);
        case_near("near: full, tight spacing", tok, task, 6, 9, 2, 4);
        case_near("near: exact step bound", tok, task, 6, 9, 2, 3);
        case_near("near: current task protected", tok, task, 6, 1, 2, 4);
        {
            const int64_t t2[6] = { 100, 101, 200, 201, 300, 400 };
            case_near("near: gaps keep entries", t2, task, 6, 9, 1, 5);
        }
    }
    expect("make_room 33/32", fg268p_make_room(33, 32), ref_make_room(33, 32), (uint64_t[1]){0}, (uint64_t[1]){0}, 1);
    expect("make_room 32/32", fg268p_make_room(32, 32), ref_make_room(32, 32), (uint64_t[1]){0}, (uint64_t[1]){0}, 1);
    expect("make_room 31/32", fg268p_make_room(31, 32), ref_make_room(31, 32), (uint64_t[1]){0}, (uint64_t[1]){0}, 1);
    expect("make_room 0/32",  fg268p_make_room(0, 32),  ref_make_room(0, 32),  (uint64_t[1]){0}, (uint64_t[1]){0}, 1);
    {
        const int64_t tok[5] = { 7, 9, 7, 11, 7 };
        uint64_t gm[1] = {0}, rm[1] = {0};
        const int64_t g = fg268p_supersede((uint64_t) (uintptr_t) tok, 5, 7, (uint64_t) (uintptr_t) gm);
        const int64_t r = ref_supersede(tok, 5, 7, rm);
        expect("supersede duplicates", g, r, gm, rm, 5);
        gm[0] = rm[0] = 0;
        const int64_t g2 = fg268p_supersede((uint64_t) (uintptr_t) tok, 5, 13, (uint64_t) (uintptr_t) gm);
        const int64_t r2 = ref_supersede(tok, 5, 13, rm);
        expect("supersede none", g2, r2, gm, rm, 5);
    }
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
