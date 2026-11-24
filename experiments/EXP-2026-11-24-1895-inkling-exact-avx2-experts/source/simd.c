#include <immintrin.h>
#include <stdint.h>
#include <string.h>

static uint16_t word(const uint8_t *data) {
    uint16_t value;
    memcpy(&value, data, sizeof value);
    return value;
}

static __m256 accumulate8(__m128i codes, __m256 scale, const float *source, __m256 sum) {
    const __m256 weights = _mm256_mul_ps(scale, _mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(codes)));
    return _mm256_fmadd_ps(weights, _mm256_loadu_ps(source), sum);
}

float i2_iq4_dot(uint64_t w, uint64_t x, int64_t width, int64_t kind) {
    const uint8_t *weights = (const void *)(uintptr_t)w;
    const float *input = (const void *)(uintptr_t)x;
    const __m128i table = _mm_setr_epi8(-127, -104, -83, -65, -49, -35, -22, -10,
                                        1, 13, 25, 38, 53, 69, 89, 113);
    const __m128i nibble = _mm_set1_epi8(15);
    __m256 a = _mm256_setzero_ps(), b = a, c = a, d = a;
    for (int64_t group = 0; group < width / 32; ++group) {
        const uint8_t *packed;
        float scale;
        if (kind == 23) {
            const unsigned index = (unsigned)group & 7;
            const uint8_t *block = weights + (group / 8) * 136;
            const unsigned lo = (block[4 + index / 2] >> (4 * (index & 1))) & 15;
            const unsigned hi = (word(block + 2) >> (2 * index)) & 3;
            scale = _cvtsh_ss(word(block)) * (float)((int)(lo + 16 * hi) - 32);
            packed = block + 8 + 16 * index;
        } else {
            const uint8_t *block = weights + group * 18;
            scale = _cvtsh_ss(word(block));
            packed = block + 2;
        }
        const __m128i bytes = _mm_loadu_si128((const __m128i *)packed);
        const __m128i low = _mm_shuffle_epi8(table, _mm_and_si128(bytes, nibble));
        const __m128i high = _mm_shuffle_epi8(table, _mm_and_si128(_mm_srli_epi16(bytes, 4), nibble));
        const __m256 factors = _mm256_set1_ps(scale);
        a = accumulate8(low, factors, input, a);
        b = accumulate8(_mm_srli_si128(low, 8), factors, input + 8, b);
        c = accumulate8(high, factors, input + 16, c);
        d = accumulate8(_mm_srli_si128(high, 8), factors, input + 24, d);
        input += 32;
    }
    const __m256 step8 = _mm256_add_ps(_mm256_add_ps(a, c), _mm256_add_ps(b, d));
    const __m128 step4 = _mm_add_ps(_mm256_castps256_ps128(step8), _mm256_extractf128_ps(step8, 1));
    const __m128 step2 = _mm_add_ps(step4, _mm_shuffle_ps(step4, step4, 0x4e));
    return _mm_cvtss_f32(_mm_add_ss(step2, _mm_shuffle_ps(step2, step2, 0xb1)));
}
