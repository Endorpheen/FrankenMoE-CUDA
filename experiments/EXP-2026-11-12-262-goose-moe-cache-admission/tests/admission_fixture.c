// Differential gate for the MoE cache admission/grouping decision.
//
// Left side: a direct transcription of the native constructor loop (llama-moe-cache.cpp, impl::impl).
// Right side: the Goose module. Both receive the same flat arrays and must agree on every output and on the verdict.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern uint8_t fg262a_run(int64_t n_layers, uint64_t n_experts, uint64_t device, uint64_t layout, uint64_t nbytes,
                          uint64_t group, uint64_t group_dev, uint64_t group_layout, uint64_t group_bytes,
                          uint64_t group_count, uint64_t n_groups, int64_t max_groups);

static int failures, checks;

static int ref_plan(int n, const int64_t * ne, const int64_t * dev, const int64_t * lay, const int64_t * nb,
                    int64_t * grp, int64_t * gdev, int64_t * glay, int64_t * gbytes, int64_t * gcount,
                    int64_t * ng, int maxg) {
    int g = 0;
    for (int il = 0; il < n; ++il) {
        int64_t found = -1;
        if (ne[il] > 0 && dev[il] >= 0 && lay[il] != 0) {
            for (int j = 0; j < g; ++j) if (gdev[j] == dev[il] && glay[j] == lay[il]) found = j;
            if (found < 0) {
                if (g >= maxg) return 0;
                found = g;
                gdev[g] = dev[il]; glay[g] = lay[il]; gbytes[g] = 0; gcount[g] = 0; g++;
            }
            gbytes[found] += nb[il];
            gcount[found]++;
        }
        grp[il] = found;
    }
    *ng = g;
    return 1;
}

static void case_run(const char * name, int n, const int64_t * ne, const int64_t * dev, const int64_t * lay,
                     const int64_t * nb, int maxg) {
    enum { MAXG = 8, MAXL = 32 };
    int64_t rgrp[MAXL], rgdev[MAXG], rglay[MAXG], rgb[MAXG], rgc[MAXG], rng = -1;
    int64_t ggrp[MAXL], ggdev[MAXG], gglay[MAXG], ggb[MAXG], ggc[MAXG], gng = -1;
    for (int i = 0; i < MAXL; ++i) { rgrp[i] = 0x77; ggrp[i] = 0x77; }
    for (int i = 0; i < MAXG; ++i) { rgdev[i] = rglay[i] = rgb[i] = rgc[i] = 0x77; ggdev[i] = gglay[i] = ggb[i] = ggc[i] = 0x77; }

    const int rv = ref_plan(n, ne, dev, lay, nb, rgrp, rgdev, rglay, rgb, rgc, &rng, maxg);
    const int gv = (int) fg262a_run(n, (uint64_t) (uintptr_t) ne, (uint64_t) (uintptr_t) dev, (uint64_t) (uintptr_t) lay,
                                    (uint64_t) (uintptr_t) nb, (uint64_t) (uintptr_t) ggrp, (uint64_t) (uintptr_t) ggdev,
                                    (uint64_t) (uintptr_t) gglay, (uint64_t) (uintptr_t) ggb, (uint64_t) (uintptr_t) ggc,
                                    (uint64_t) (uintptr_t) &gng, maxg);
    checks++;
    int bad = 0;
    if (rv != gv) { printf("%-24s verdict ref=%d goose=%d MISMATCH\n", name, rv, gv); bad = 1; }
    if (rv) {
        for (int i = 0; i < n; ++i) if (rgrp[i] != ggrp[i]) { printf("%-24s group[%d] ref=%lld goose=%lld\n", name, i, (long long) rgrp[i], (long long) ggrp[i]); bad = 1; }
        if (rng != gng) { printf("%-24s n_groups ref=%lld goose=%lld\n", name, (long long) rng, (long long) gng); bad = 1; }
        for (int j = 0; j < rng; ++j) {
            if (rgdev[j] != ggdev[j] || rglay[j] != gglay[j] || rgb[j] != ggb[j] || rgc[j] != ggc[j]) {
                printf("%-24s group %d ref=(%lld,%lld,%lld,%lld) goose=(%lld,%lld,%lld,%lld)\n", name, j,
                       (long long) rgdev[j], (long long) rglay[j], (long long) rgb[j], (long long) rgc[j],
                       (long long) ggdev[j], (long long) gglay[j], (long long) ggb[j], (long long) ggc[j]);
                bad = 1;
            }
        }
    }
    if (!bad) printf("%-24s OK\n", name);
    failures += bad;
}

int main(void) {
    {   // nothing to admit
        const int64_t ne[3] = { 0, 0, 0 }, dev[3] = { 0, 1, 0 }, lay[3] = { 1, 2, 3 }, nb[3] = { 10, 20, 30 };
        case_run("all skipped", 3, ne, dev, lay, nb, 8);
    }
    {   // rejected for a different reason on each layer
        const int64_t ne[4] = { 8, 0, 8, 8 }, dev[4] = { -1, 0, 0, 0 }, lay[4] = { 5, 5, 0, 5 }, nb[4] = { 1, 2, 3, 4 };
        case_run("three skip reasons", 4, ne, dev, lay, nb, 8);
    }
    {   // one group from three layers, bytes and count summed
        const int64_t ne[3] = { 8, 8, 8 }, dev[3] = { 0, 0, 0 }, lay[3] = { 5, 5, 5 }, nb[3] = { 100, 200, 300 };
        case_run("one group of three", 3, ne, dev, lay, nb, 8);
    }
    {   // same layout on two devices must be two groups
        const int64_t ne[2] = { 8, 8 }, dev[2] = { 0, 1 }, lay[2] = { 5, 5 }, nb[2] = { 100, 200 };
        case_run("layout split by device", 2, ne, dev, lay, nb, 8);
    }
    {   // same device, two layouts must be two groups
        const int64_t ne[3] = { 8, 8, 8 }, dev[3] = { 0, 0, 0 }, lay[3] = { 5, 6, 5 }, nb[3] = { 10, 20, 30 };
        case_run("layouts on one device", 3, ne, dev, lay, nb, 8);
    }
    {   // mixed with an interleaved admitted layer
        const int64_t ne[5] = { 8, 0, 8, 8, 0 }, dev[5] = { 0, 0, 1, 0, 1 }, lay[5] = { 5, 5, 5, 5, 5 }, nb[5] = { 1, 2, 3, 4, 5 };
        case_run("mixed admitted/skipped", 5, ne, dev, lay, nb, 8);
    }
    {   // capacity: the reference refuses before writing the second group
        const int64_t ne[2] = { 8, 8 }, dev[2] = { 0, 1 }, lay[2] = { 5, 5 }, nb[2] = { 1, 2 };
        case_run("capacity exceeded", 2, ne, dev, lay, nb, 1);
    }
    {   // zero layers is a valid empty plan
        case_run("no layers", 0, NULL, NULL, NULL, NULL, 8);
    }
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
