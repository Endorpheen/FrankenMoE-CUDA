#include "main.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define Q35_TOPK_AVX2 1
#else
#define Q35_TOPK_AVX2 0
#endif

uint8_t fg_q35_topk_scan_enabled(void) {
#if Q35_TOPK_AVX2
    const char * setting = getenv("GOOSE_CPU_TOPK_SCAN");
    return (!setting || strcmp(setting, "0") != 0) && __builtin_cpu_supports("avx2");
#else
    return 0;
#endif
}

#if Q35_TOPK_AVX2
static __attribute__((target("avx2"))) int64_t first_above_avx2(const float * values,
        int64_t start, int64_t end, float threshold) {
    const __m256 cutoff = _mm256_set1_ps(threshold);
    for (; end - start >= 32; start += 32) {
        uint32_t mask = (uint32_t)_mm256_movemask_ps(_mm256_cmp_ps(_mm256_loadu_ps(values + start), cutoff, _CMP_GT_OQ));
        mask |= (uint32_t)_mm256_movemask_ps(_mm256_cmp_ps(_mm256_loadu_ps(values + start + 8), cutoff, _CMP_GT_OQ)) << 8;
        mask |= (uint32_t)_mm256_movemask_ps(_mm256_cmp_ps(_mm256_loadu_ps(values + start + 16), cutoff, _CMP_GT_OQ)) << 16;
        mask |= (uint32_t)_mm256_movemask_ps(_mm256_cmp_ps(_mm256_loadu_ps(values + start + 24), cutoff, _CMP_GT_OQ)) << 24;
        if (mask) return start + __builtin_ctz(mask);
    }
    for (; start < end; ++start) {
        if (values[start] > threshold) return start;
    }
    return end;
}
#endif

int64_t fg_q35_topk_first_above(sl_f32 values, int64_t start, int64_t end, float threshold) {
#if Q35_TOPK_AVX2
    return first_above_avx2(values.data, start, end, threshold);
#else
    (void)values;
    (void)start;
    (void)threshold;
    return end;
#endif
}
