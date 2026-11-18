// Corrected repro: every group, independent scalar q4_K x q8_K dot as reference.
// The shipped repro called ggml_vec_dot_q4_K_q8_K with block strides of 0 and got
// 0 for every row; here the reference is computed directly from the block format.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

struct args_t {
    const uint8_t * src0; int64_t ne00, ne01, nb01;
    const uint8_t * act;  int64_t nbw1;
    const int32_t * routes; int64_t cne1;
    int64_t nb0, nb1, nb2;
    float * dst; int32_t ith, nth;
};

extern "C" int32_t fg_cpu_simd_mmid_tiled_q4_k(const args_t *, uint8_t *);

static float f16(const uint8_t * p) {
    uint32_t h = (uint32_t) p[0] | ((uint32_t) p[1] << 8);
    uint32_t sign = (h & 0x8000u) << 16;
    uint32_t e = (h >> 10) & 31u;
    uint32_t f = h & 1023u;
    uint32_t bits;
    if (e == 31u) bits = sign | 2139095040u | (f << 13);
    else if (e == 0u) {
        if (f == 0u) bits = sign;
        else {
            int32_t power = -14;
            while ((f & 1024u) == 0u) { f <<= 1; power -= 1; }
            f &= 1023u;
            bits = sign | (((uint32_t) (power + 127)) << 23) | (f << 13);
        }
    } else bits = sign | ((e + 112u) << 23) | (f << 13);
    float v; memcpy(&v, &bits, 4); return v;
}

static uint32_t u32(const uint8_t * p) {
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

static float scalar_dot(const uint8_t * w, const uint8_t * a, int64_t k) {
    const uint32_t km1 = 0x3F3F3F3Fu, km2 = 0x0F0F0F0Fu, km3 = 0x03030303u;
    float total = 0.0f;
    for (int64_t kb = 0; kb < k / 256; kb++) {
        const uint8_t * blk = w + kb * 144;
        const float d = f16(blk), dmin = f16(blk + 2);
        uint32_t u0 = u32(blk + 4), u1 = u32(blk + 8), u2 = u32(blk + 12);
        uint32_t u3 = ((u2 >> 4) & km2) | (((u1 >> 6) & km3) << 4);
        uint32_t aux = u1 & km1;
        uint32_t u1n = (u2 & km2) | (((u0 >> 6) & km3) << 4);
        uint32_t u0n = u0 & km1;
        int32_t sc[8], mn[8];
        for (int s = 0; s < 4; s++) {
            sc[s] = (int32_t) ((u0n >> (8 * s)) & 255u);
            sc[4 + s] = (int32_t) ((u1n >> (8 * s)) & 255u);
            mn[s] = (int32_t) ((aux >> (8 * s)) & 255u);
            mn[4 + s] = (int32_t) ((u3 >> (8 * s)) & 255u);
        }
        const uint8_t * qb = blk + 16;
        const uint8_t * ab = a + kb * 292;
        const float da = *(const float *) ab;
        const int8_t * q1 = (const int8_t *) (ab + 4);
        const int16_t * bs = (const int16_t *) (ab + 260);
        int32_t s1 = 0, s2 = 0;
        for (int s = 0; s < 8; s++) {
            int32_t sub = 0;
            for (int e = 0; e < 32; e++) {
                const int g = s * 32 + e;
                const uint8_t byte = qb[g >> 1];
                const int q0 = (g & 1) ? ((byte >> 4) & 15) : (byte & 15);
                // nibble order: subblock s uses qs[32*(s/2)], low/high by parity
                (void) q0;
            }
            // correct nibble layout: 4 groups of 32 bytes, lo then hi
            int32_t acc = 0;
            const uint8_t * src = qb + 32 * (s / 2);
            for (int e = 0; e < 32; e++) {
                const int v = (s & 1) ? ((src[e] >> 4) & 15) : (src[e] & 15);
                acc += v * (int32_t) q1[s * 32 + e];
            }
            sub = acc;
            s1 = (int32_t) ((uint32_t) s1 + (uint32_t) (sc[s] * sub));
            s2 = (int32_t) ((uint32_t) s2 + (uint32_t) (mn[s] * (int32_t) (bs[2 * s] + bs[2 * s + 1])));
        }
        float res = d * (float) s1;
        res = __builtin_fmaf(-dmin, (float) s2, res);
        total = __builtin_fmaf(da, res, total);
    }
    return total;
}

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "evidence-mmid/failing_case.bin";
    FILE * f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    int64_t h[10];
    if (fread(h, 8, 10, f) != 10) return 2;
    const int64_t K = h[0], R = h[1], nb01 = h[2], nbw1 = h[3], cne1 = h[4];
    const int64_t nb0 = h[5], nb1 = h[6], nb2 = h[7];
    std::vector<uint8_t> src0((size_t) (R * nb01)), act((size_t) (cne1 * nbw1));
    if (fread(src0.data(), 1, src0.size(), f) != src0.size()) return 2;
    if (fread(act.data(), 1, act.size(), f) != act.size()) return 2;
    std::vector<int32_t> routes((size_t) (2 * cne1));
    if (fread(routes.data(), 4, routes.size(), f) != routes.size()) return 2;
    fclose(f);

    int64_t bad_total = 0;
    const int64_t ngroups = (R + 63) / 64;
    for (int64_t g = 0; g < ngroups; g++) {
        const int64_t r0 = g * 64, r1 = std::min(r0 + 64, R);
        const int64_t n = r1 - r0;
        std::vector<uint8_t> out((size_t) (n * nb0 + ((int64_t) routes[0] + 2) * nb1 +
                                           ((int64_t) routes[1] + 2) * nb2 + 4096), 0);
        std::vector<uint8_t> scratch(16u * 512u * 1024u, 0);
        // one call per group, as the runtime splits the groups over threads
        const int64_t g0 = g, g1 = g + 1;
        (void) g0; (void) g1;
        args_t a{ src0.data(), K, R, nb01, act.data(), nbw1, routes.data(), cne1,
                  nb0, nb1, nb2, (float *) out.data(), 0, 1 };
        // restrict this call to one group by using a kernel-local view: run all and
        // read only this group's rows
        fg_cpu_simd_mmid_tiled_q4_k(&a, scratch.data());
        int64_t eq = 0, bad = 0, first = -1;
        for (int64_t r = r0; r < r1; r++) {
            for (int64_t m = 0; m < cne1; m++) {
                const int64_t off = r * nb0 + (int64_t) routes[2 * m] * nb1 + (int64_t) routes[2 * m + 1] * nb2;
                float got; memcpy(&got, out.data() + off, 4);
                const float ref = scalar_dot(src0.data() + r * nb01, act.data() + m * nbw1, K);
                uint32_t x, y; memcpy(&x, &got, 4); memcpy(&y, &ref, 4);
                if (x == y) eq++;
                else { bad++; if (first < 0) first = r; }
            }
        }
        bad_total += bad;
        printf("group=%lld rows=[%lld,%lld) eq=%lld/%lld first_diff=%lld\n",
               (long long) g, (long long) r0, (long long) r1, (long long) eq,
               (long long) (eq + bad), (long long) first);
    }
    printf("total diffs=%lld\n", (long long) bad_total);
    return bad_total ? 1 : 0;
}
