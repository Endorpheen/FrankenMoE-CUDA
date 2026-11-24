#include "cuda-driver.c"

#include <math.h>

static uint64_t checks;
static uint64_t unequal;
static double sum_error;
static float max_error;

int64_t i2_cuda_shadow_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) {
        enabled = getenv("I2_VERIFY_DEVICE_TENSOR") != NULL;
    }
    return enabled;
}

void i2_cuda_shadow_compare(sl_f32 candidate, sl_f32 reference) {
    if (candidate.len != reference.len) {
        abort();
    }
    for (int64_t i = 0; i < candidate.len; ++i) {
        const float a = candidate.data[i];
        const float b = reference.data[i];
        const float error = fabsf(a - b);
        if (!isfinite(a) || !isfinite(b)) {
            abort();
        }
        if (memcmp(&a, &b, sizeof(float)) != 0) {
            ++unequal;
        }
        sum_error += error;
        if (error > max_error) {
            max_error = error;
        }
    }
    checks += candidate.len;
}

void i2_cuda_shadow_stats(void) {
    printf("Inkling shadow elements=%" PRIu64 " unequal=%" PRIu64 " max_abs=%.9g mean_abs=%.9g\n",
           checks, unequal, max_error, checks ? sum_error / checks : 0.0);
}
