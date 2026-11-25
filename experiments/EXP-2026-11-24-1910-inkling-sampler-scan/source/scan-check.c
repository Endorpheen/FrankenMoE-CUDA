#include "main.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

extern int64_t fg_i2_sampler_first_above(sl_f32 values, int64_t start, int64_t count, float floor);
extern int64_t fg_i2_sampler_init_ids(sl_i64 ids, int64_t count);

uint8_t fg_cuda_inkling_quants_enabled(void) { return 1; }

int main(void) {
    enum { size = 1029 };
    float span[size];
    int64_t ids[size];
    uint32_t rng = 83;
    for (int i = 0; i < size; ++i) {
        rng = rng * 1664525u + 1013904223u;
        span[i] = (float)((int)(rng % 7) - 3);
    }
    span[7] = NAN;
    span[16] = INFINITY;
    span[97] = -INFINITY;
    span[279] = -0.0f;
    int checks = 0;
    const float cutoffs[] = {-INFINITY, -3.0f, -1.0f, -0.0f, 2.0f, 3.0f, INFINITY, NAN};
    for (int c = 0; c < 8; ++c) {
        for (int lo = 0; lo < size; lo += 3) {
            for (int hi = lo; hi <= size; hi += 7) {
                int want = lo;
                while (want < hi && !(span[want] > cutoffs[c])) ++want;
                assert(fg_i2_sampler_first_above((sl_f32){span, size}, lo, hi, cutoffs[c]) == want);
                ++checks;
            }
        }
    }
    assert(fg_i2_sampler_init_ids((sl_i64){ids, size}, size) == 0);
    for (int i = 0; i < size; ++i) assert(ids[i] == i);
    printf("%d ordered scan checks passed\n", checks);
}
