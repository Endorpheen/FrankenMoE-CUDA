#include "main.h"
#include <stdbool.h>
#include <stdlib.h>

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>

__attribute__((target("avx2")))
static int64_t first_above_avx2(const float *values, int64_t begin,
                                int64_t end, float floor) {
    const __m256 threshold = _mm256_set1_ps(floor);
    int64_t i = begin;
    for (; end - i >= 8; i += 8) {
        const __m256 candidate = _mm256_loadu_ps(values + i);
        const unsigned mask = (unsigned)_mm256_movemask_ps(_mm256_cmp_ps(candidate, threshold, _CMP_GT_OQ));
        if (mask) return i + __builtin_ctz(mask);
    }
    for (; i < end; ++i) if (values[i] > floor) return i;
    return end;
}
#endif

extern uint8_t fg_cuda_inkling_quants_enabled(void);

uint8_t fg_i2_sampler_scan_enabled(int64_t count) {
    const char *mode = getenv("I2_SAMPLER_SCAN");
    return count >= 1024 && mode && mode[0] == '1' && mode[1] == '\0' && fg_cuda_inkling_quants_enabled();
}

int64_t fg_i2_sampler_first_above(sl_f32 values, int64_t start, int64_t count, float floor) {
    if (start < 0 || count < start || count > values.len || !values.data) return count;
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    if (__builtin_cpu_supports("avx2")) return first_above_avx2(values.data, start, count, floor);
#endif
    for (int64_t i = start; i < count; ++i) if (values.data[i] > floor) return i;
    return count;
}

int64_t fg_i2_sampler_init_ids(sl_i64 ids, int64_t count) {
    if (!ids.data || count < 0 || count > ids.len) return -1;
    for (int64_t i = 0; i < count; ++i) ids.data[i] = i;
    return 0;
}
