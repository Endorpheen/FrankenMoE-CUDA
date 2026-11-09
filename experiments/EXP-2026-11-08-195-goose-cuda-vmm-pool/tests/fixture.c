// Differential gate for the Goose VMM pool arithmetic.
//
// The reference is a line-by-line copy of ggml_cuda_pool_vmm::alloc/free from ggml-cuda.cu,
// with the driver calls removed, so the comparison is against the native algorithm and not
// against a re-derivation of it.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define MAX_SIZE (1ull << 35)

// ---- native reference, transcribed from ggml-cuda.cu -------------------------------
struct nat { unsigned long long pool_addr_off, pool_used, pool_size; };

static void nat_alloc(struct nat *n, size_t size, size_t granularity,
                      size_t *actual, unsigned long long *reserve_out, int *grew) {
    const size_t alignment = 128;
    size = alignment * ((size + alignment - 1) / alignment);
    size_t avail = n->pool_size - n->pool_used;
    *reserve_out = 0; *grew = 0;
    if (size > avail) {
        size_t reserve_size = size - avail;
        reserve_size = granularity * ((reserve_size + granularity - 1) / granularity);
        if (n->pool_size + reserve_size > MAX_SIZE) { fprintf(stderr, "native overflow\n"); exit(3); }
        n->pool_size += reserve_size;
        *reserve_out = reserve_size; *grew = 1;
    }
    *actual = size;
    n->pool_addr_off = n->pool_used;
    n->pool_used += size;
}

static void nat_free(struct nat *n, unsigned long long ptr_off, size_t size) {
    n->pool_used -= size;
    if (ptr_off != n->pool_used) { fprintf(stderr, "native free order violated\n"); exit(4); }
}

// ---- Goose entry points ------------------------------------------------------------
void fg195v_plan_alloc(int64_t size, int64_t pool_used, int64_t pool_size,
                       int64_t granularity, uint64_t out);
int64_t fg195v_plan_free(int64_t ptr_off, int64_t pool_used, int64_t size);

static int checks, failures;
static void ck(int ok, const char *what) { ++checks; if (!ok) { ++failures; fprintf(stderr, "FAIL %s\n", what); } }

static unsigned rs = 195001;
static unsigned nx(void) { rs = rs * 1664525u + 1013904223u; return rs; }

int main(void) {
    const size_t grans[] = { 2u << 20, 16u << 20 };
    for (unsigned g = 0; g < sizeof(grans)/sizeof(grans[0]); ++g) {
        const size_t gran = grans[g];
        struct nat n = {0, 0, 0};
        for (int iter = 0; iter < 400; ++iter) {
            // mixture of small, large and exactly-borderline requests
            size_t size;
            switch (nx() % 5) {
                case 0: size = 1; break;
                case 1: size = 128; break;
                case 2: size = 1 + nx() % 4096; break;
                case 3: size = 1 + nx() % (2u << 20); break;
                default: size = 4096 + nx() % (1u << 20); break;
            }
            size_t actual_n = 0; unsigned long long reserve_n = 0; int grew_n = 0;
            nat_alloc(&n, size, gran, &actual_n, &reserve_n, &grew_n);

            uint64_t out[8] = {0};
            fg195v_plan_alloc((int64_t)size, (int64_t)0, (int64_t)0, (int64_t)gran, (uint64_t)(uintptr_t)out);
            // rerun with the real pool state, since the first call above only checked the guards
            struct nat snapshot = n;
            snapshot.pool_used = n.pool_used - actual_n;   // state before this alloc
            snapshot.pool_size = n.pool_size - reserve_n;
            uint64_t o2[8] = {0};
            fg195v_plan_alloc((int64_t)size, (int64_t)snapshot.pool_used, (int64_t)snapshot.pool_size,
                              (int64_t)gran, (uint64_t)(uintptr_t)o2);
            ck((unsigned long long)o2[0] == reserve_n, "reserve matches native");
            ck(o2[1] == snapshot.pool_used, "pointer offset matches native");
            ck((unsigned long long)o2[2] == n.pool_size, "pool_size matches native");
            ck((unsigned long long)o2[3] == n.pool_used, "pool_used matches native");
            ck((unsigned long long)o2[4] == actual_n, "actual_size matches native");
            ck((unsigned long long)o2[5] == snapshot.pool_size, "map offset is the pre-growth pool_size");

            // free immediately, exercising the reverse-order check
            int64_t got = fg195v_plan_free((int64_t)snapshot.pool_used, (int64_t)n.pool_used, (int64_t)actual_n);
            nat_free(&n, snapshot.pool_used, actual_n);
            ck((unsigned long long)got == n.pool_used, "free rewinds like native");
        }
    }
    printf("FG195V_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
