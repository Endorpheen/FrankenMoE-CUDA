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

// exact transcription of ggml-cpu's quants.c generic kernel, the source in this tree
static float f16(uint32_t h);
extern "C" void ggml_vec_dot_q4_K_q8_K(int, float *, size_t, const void *, size_t, const void *, size_t, int);
static float ref_generic(const block_q4_K * x, const block_q8_K * y, int nb) {
    float sums[8]; std::memset(sums, 0, sizeof(sums)); float sumf = 0.0f;
    const uint32_t m1 = 0x3f3f3f3f, m2 = 0x0f0f0f0f, m3 = 0x03030303;
    for (int i = 0; i < nb; ++i) {
        int32_t aux32[8]; std::memset(aux32, 0, sizeof(aux32));
        uint32_t utmp[4] = {0,0,0,0}; std::memcpy(utmp, x[i].scales, 12);
        utmp[3] = ((utmp[2] >> 4) & m2) | (((utmp[1] >> 6) & m3) << 4);
        const uint32_t ua = utmp[1] & m1;
        utmp[1] = (utmp[2] & m2) | (((utmp[0] >> 6) & m3) << 4);
        utmp[2] = ua; utmp[0] &= m1;
        const uint8_t * scales = (const uint8_t *) &utmp[0];
        const uint8_t * mins   = (const uint8_t *) &utmp[2];
        int sumi = 0; for (int j = 0; j < 16; ++j) sumi += y[i].bsums[j]*mins[j/2];
        int8_t a[256]; const uint8_t * q4 = x[i].qs; int8_t * ap = a;
        for (int j = 0; j < 4; ++j) {
            for (int l = 0; l < 32; ++l) ap[l] = (int8_t)(q4[l] & 0xF); ap += 32;
            for (int l = 0; l < 32; ++l) ap[l] = (int8_t)(q4[l] >> 4); ap += 32; q4 += 32;
        }
        const int8_t * q8 = y[i].qs; ap = a; int is = 0;
        for (int j = 0; j < 8; ++j) {
            int32_t scale = scales[is++];
            for (int g = 0; g < 4; ++g) {
                int16_t aux16[8];
                for (int l = 0; l < 8; ++l) aux16[l] = q8[l]*ap[l];
                for (int l = 0; l < 8; ++l) aux32[l] += scale*aux16[l];
                q8 += 8; ap += 8;
            }
        }
        const float d = f16(x[i].d) * y[i].d;
        for (int l = 0; l < 8; ++l) sums[l] += d * aux32[l];
        const float dmin = f16(x[i].dmin) * y[i].d;
        sumf -= dmin * sumi;
    }
    for (int l = 0; l < 8; ++l) sumf += sums[l];
    return sumf;
}

static float f16(uint32_t h);
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
            float want = 0.0f; ggml_vec_dot_q4_K_q8_K(256*nb, &want, 0, x.data(), 0, y.data(), 0, 1);
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
