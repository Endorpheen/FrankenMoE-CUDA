#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
extern "C" float fg410s_dot(int64_t nb, const void * q4, const void * q8);
extern "C" int fg410s_enabled(void);
extern "C" void ggml_cpu_init(void);
extern "C" void ggml_vec_dot_q4_K_q8_K_generic(int n, float * s, size_t bs, const void * vx, size_t bx,
                                                const void * vy, size_t by, int nrc);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
static uint32_t bits(float v) { uint32_t u; std::memcpy(&u, &v, 4); return u; }

struct block_q4_K { uint16_t d, dmin; uint8_t scales[12]; uint8_t qs[128]; };
struct block_q8_K { float d; int8_t qs[256]; int16_t bsums[16]; };
static_assert(sizeof(block_q4_K) == 144, "q4_K size");
static_assert(sizeof(block_q8_K) == 292, "q8_K size");

static float f16(uint32_t h) { // small helper for test data
    uint32_t s=(h>>15)&1, e=(h>>10)&0x1f, m=h&0x3ff, o;
    if (e==0) { if (m==0) o=s<<31; else { int sh=0; while(!(m&0x400)){m<<=1;++sh;} m&=0x3ff; o=(s<<31)|((uint32_t)(127-15-sh)<<23)|(m<<13);} }
    else if (e==31) o=(s<<31)|0x7f800000|(m<<13);
    else o=(s<<31)|((e+112)<<23)|(m<<13);
    float f; std::memcpy(&f,&o,4); return f;
}

int main(void) {
    ggml_cpu_init();   // without it the reference returns 0 for every input
    check(fg410s_enabled() == 1, "enabled");
    std::mt19937 rng(410);
    int cases = 0;
    for (int nb : {1, 2, 3, 7}) {
        for (int trial = 0; trial < 40; ++trial) {
            std::vector<block_q4_K> x(nb);
            std::vector<block_q8_K> y(nb);
            for (int i = 0; i < nb; ++i) {
                x[i].d = (uint16_t)(0x3000 | (rng() & 0x3ff));      // ~0.125
                x[i].dmin = (uint16_t)(0x2c00 | (rng() & 0x3ff));   // ~0.06
                for (auto & b : x[i].scales) b = (uint8_t)rng();
                for (auto & b : x[i].qs) b = (uint8_t)rng();
                y[i].d = 0.001f * (float)(1 + (rng() % 1000));
                for (auto & b : y[i].qs) b = (int8_t)rng();
                for (auto & s : y[i].bsums) s = (int16_t)((rng() % 2001) - 1000);
            }
            float want = 0.0f;
            ggml_vec_dot_q4_K_q8_K_generic(256 * nb, &want, 0, x.data(), 0, y.data(), 0, 1);
            const float got = fg410s_dot(nb, x.data(), y.data());
            ++cases;
            if (bits(got) != bits(want) && failures == 0) {
                FILE * f = fopen("/tmp/fail_blocks.bin", "wb");
                if (f) { fwrite(&nb, sizeof(nb), 1, f); fwrite(x.data(), sizeof(block_q4_K), nb, f);
                         fwrite(y.data(), sizeof(block_q8_K), nb, f); fclose(f); }
            }
            if (bits(got) != bits(want) && failures < 3)
                std::fprintf(stderr, "MISMATCH nb=%d trial=%d got=%.9g want=%.9g\n", nb, trial, got, want);
            check(bits(got) == bits(want), "bit-exact");
            (void) f16;
        }
    }
    std::printf("FG410S_ADAPTOR cases=%d checks=%d failures=%d\n", cases, checks, failures);
    return failures ? 1 : 0;
}
