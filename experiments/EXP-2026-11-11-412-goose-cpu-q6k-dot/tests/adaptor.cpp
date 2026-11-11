// Bit-exact gate for the Goose Q6_K x Q8_K dot, against the library's dispatched kernel.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

extern "C" void ggml_cpu_init(void);          // without it the reference returns 0
extern "C" void ggml_vec_dot_q6_K_q8_K(int, float *, size_t, const void *, size_t, const void *, size_t, int);
extern "C" float fg412s_dot(int64_t nb, const void * x, const void * y);

struct block_q6_K { uint8_t ql[128]; uint8_t qh[64]; int8_t scales[16]; uint16_t d; };
struct block_q8_K { float d; int8_t qs[256]; int16_t bsums[16]; };
static_assert(sizeof(block_q6_K) == 210, "q6_K size");
static_assert(sizeof(block_q8_K) == 292, "q8_K size");

static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

static uint16_t hbits(float f) {
    uint32_t u; std::memcpy(&u, &f, 4);
    uint32_t sign = (u >> 16) & 0x8000;
    int32_t exp = (int32_t)((u >> 23) & 0xff) - 112;
    uint32_t man = (u >> 13) & 0x3ff;
    if (exp <= 0) return (uint16_t)sign;
    if (exp >= 31) return (uint16_t)(sign | 0x7c00);
    return (uint16_t)(sign | ((uint32_t)exp << 10) | man);
}

int main() {
    ggml_cpu_init();
    std::mt19937 rng(412);
    int cases = 0, failures = 0;
    for (int nb : {1, 2, 3, 7, 8, 16}) {
        for (int trial = 0; trial < 40; ++trial) {
            std::vector<block_q6_K> x(nb);
            std::vector<block_q8_K> y(nb);
            for (int i = 0; i < nb; ++i) {
                x[i].d = hbits(0.05f + 0.2f * (float)(rng() % 100) / 100.0f);
                for (auto & b : x[i].ql) b = (uint8_t)rng();
                for (auto & b : x[i].qh) b = (uint8_t)rng();
                for (auto & s : x[i].scales) s = (int8_t)((int)(rng() % 201) - 100);
                y[i].d = 0.001f * (float)(1 + (rng() % 1000));
                for (auto & b : y[i].qs) b = (int8_t)rng();
                std::memset(y[i].bsums, 0, sizeof(y[i].bsums));
                for (int k = 0; k < 256; ++k) y[i].bsums[k / 16] += y[i].qs[k];
            }
            float want = 0.0f;
            ggml_vec_dot_q6_K_q8_K(256 * nb, &want, 0, x.data(), 0, y.data(), 0, 1);
            const float got = fg412s_dot(nb, x.data(), y.data());
            ++cases;
            if (bits(got) != bits(want)) {
                if (failures < 3)
                    std::fprintf(stderr, "MISMATCH nb=%d trial=%d got=%.9g want=%.9g\n", nb, trial, got, want);
                ++failures;
            }
        }
    }
    std::printf("FG412_ADAPTOR cases=%d failures=%d\n", cases, failures);
    return failures ? 1 : 0;
}
