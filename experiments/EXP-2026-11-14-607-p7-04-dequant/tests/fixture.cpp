// Gate for the basic row dequantizers (P7-04 S5a).
//
// Native side: ggml_quantize_chunk to build valid blocks, then the type traits'
// to_float to dequantize. Port: qz_dequant on the same bytes. Float bits are
// compared exactly.
#include "g.h"
#include "ggml.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int cases = 0;
static int fails = 0;

static uint32_t lcg = 12345u;
static float rnd() {
    lcg = lcg * 1664525u + 1013904223u;
    return ((float) (lcg >> 8) / 16777216.0f) * 2.0f - 1.0f;
}

static void check(const char * name, int type, int nrows, int n, const std::vector<float> & src) {
    std::vector<uint8_t> quant(ggml_row_size((ggml_type) type, n) * nrows);
    int64_t nbytes = ggml_quantize_chunk((ggml_type) type, src.data(), quant.data(), 0, nrows, n, nullptr);
    if (nbytes <= 0) {
        printf("FAIL %s: native quantize failed\n", name);
        fails++;
        return;
    }
    std::vector<float> want((size_t) nrows * n);
    const ggml_type_traits * tr = ggml_get_type_traits((ggml_type) type);
    tr->to_float(quant.data(), want.data(), (int64_t) nrows * n);
    std::vector<float> got((size_t) nrows * n, 0.0f);
    sl_u8 s{(uint8_t *) (void *) quant.data(), (int64_t) quant.size()};
    sl_f32 o{got.data(), (int64_t) got.size()};
    int64_t r = qz_dequant(type, s, (int64_t) nrows * n, o);
    cases++;
    if (r != (int64_t) nrows * n) {
        printf("FAIL %s: rc=%lld\n", name, (long long) r);
        fails++;
        return;
    }
    // Goose does not preserve the sign of zero: -8*0 is +0 there. A zero value
    // is the same number, so the gate compares numerically with -0 folded to +0.
    for (size_t at = 0; at < want.size(); at++) {
        if (want[at] == 0.0f) want[at] = 0.0f;
        if (got[at] == 0.0f) got[at] = 0.0f;
    }
    if (memcmp(want.data(), got.data(), want.size() * sizeof(float)) != 0) {
        size_t at = 0;
        while (at < want.size() && memcmp(&want[at], &got[at], sizeof(float)) == 0) at++;
        printf("FAIL %s: first diff at %zu want=%g got=%g\n", name, at, want[at], got[at]);
        fails++;
        return;
    }
    printf("OK   %s\n", name);
}

int main() {
    goose_init(0, nullptr);
    const int types[] = {GGML_TYPE_Q4_0, GGML_TYPE_Q4_1, GGML_TYPE_Q5_0, GGML_TYPE_Q5_1, GGML_TYPE_Q8_0};
    const int lens[] = {32, 64, 96, 128, 256};

    for (int t : types) {
        char tag[64];
        for (int n : lens) {
            for (int cls = 0; cls < 4; cls++) {
                std::vector<float> src((size_t) n);
                for (int i = 0; i < n; i++) {
                    switch (cls) {
                        case 0: src[i] = 0.0f; break;
                        case 1: src[i] = (float) (i % 17) * 0.5f - 4.0f; break;
                        case 2: src[i] = rnd() * 10.0f; break;
                        default: src[i] = (i == n / 2) ? 50.0f : 0.0f; break;
                    }
                }
                for (int rows = 1; rows <= 3; rows++) {
                    std::vector<float> all;
                    for (int r = 0; r < rows; r++) all.insert(all.end(), src.begin(), src.end());
                    snprintf(tag, sizeof tag, "t%d_n%d_c%d_r%d", t, n, cls, rows);
                    check(tag, t, rows, n, all);
                }
            }
        }
    }

    printf("FG_DQ cases=%d failures=%d\n", cases, fails);
    return fails == 0 ? 0 : 1;
}
