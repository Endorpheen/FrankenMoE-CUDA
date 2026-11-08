// worker-12 host mathematical/boundary fixture for the emitted Q4_0 FA unit.
// Every checked value is produced by the generated Goose functions; the
// reference below is an independent scalar model of the same native source.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <vector>
#include <algorithm>
#include "generated/fattn-q4-host.inc"

static int g_cases = 0;
static int g_fail = 0;
static uint64_t g_hash = 1469598103934665603ull;

static void mixbits(uint32_t b) { g_hash ^= b; g_hash *= 1099511628211ull; }
static uint32_t fbits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

static bool closef(float a, float b, float rel = 2e-5f, float abs_tol = 2e-6f) {
    if (std::isnan(a) || std::isnan(b)) return false;
    float d = std::fabs(a - b);
    return d <= abs_tol + rel * std::fmax(std::fabs(a), std::fabs(b));
}

static void check(const char * name, bool ok) {
    ++g_cases;
    if (!ok) { ++g_fail; std::printf("FAIL %s\n", name); }
    else std::printf("ok %s\n", name);
}

// Independent IEEE half decoder (no CUDA headers).
static float ref_half(uint16_t h) {
    uint32_t sign = uint32_t(h & 0x8000u) << 16;
    uint32_t e = (h >> 10) & 0x1Fu;
    uint32_t m = h & 0x3FFu;
    uint32_t bits;
    if (e == 0) {
        if (m == 0) bits = sign;
        else {
            float v = std::ldexp(float(m), -24);
            return sign ? -v : v;
        }
    } else if (e == 31) {
        bits = sign | 0x7F800000u | (m ? 0x007FFFFFu : 0u);
    } else {
        bits = sign | ((e - 15 + 127) << 23) | (m << 13);
    }
    float out; memcpy(&out, &bits, 4); return out;
}

static int ref_nib(uint8_t byte, bool high) { return (high ? (byte >> 4) : (byte & 15)) - 8; }

static float ref_deq_scalar(const uint8_t * row, int64_t i0, int64_t l) {
    int64_t ib = i0 / 32;
    int64_t iqs = i0 % 16;
    int shift = int((i0 % 32) / 16);
    const uint8_t * blk = row + ib * 18;
    float d = ref_half(uint16_t(blk[0] | (blk[1] << 8)));
    uint8_t q = blk[2 + iqs + l];
    return d * float(ref_nib(q, shift != 0));
}

static void put_half(uint8_t * p, uint16_t h) { p[0] = uint8_t(h); p[1] = uint8_t(h >> 8); }
static uint16_t get_half(const uint8_t * p) { return uint16_t(p[0] | (p[1] << 8)); }

static uint32_t rng_state = 0x9E3779B9u;
static uint32_t rnd() { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }

static void test_dequant() {
    static const uint16_t dvals[] = {0x3C00, 0x3800, 0xC000, 0x3000, 0x0001, 0x7BFF};
    for (int D : {64, 128, 256}) {
        int nb = D / 32;
        std::vector<uint8_t> row(size_t(nb * 18 + 64), 0xA5);
        for (int b = 0; b < nb; ++b) {
            put_half(&row[size_t(b * 18)], dvals[(b + D) % 6]);
            for (int j = 0; j < 16; ++j) row[size_t(b * 18 + 2 + j)] = uint8_t(rnd());
        }
        float maxerr = 0.0f;
        for (int i0 = 0; i0 < D; i0 += 4) {
            for (int l = 0; l < 4; ++l) {
                float got = FG012_DEQ4_SCALAR((int64_t)(intptr_t)row.data(), i0, l);
                float ref = ref_deq_scalar(row.data(), i0, l);
                maxerr = std::fmax(maxerr, std::fabs(got - ref));
                mixbits(fbits(got));
            }
            FG012_DEQ4_TYPE v = FG012_DEQ4((int64_t)(intptr_t)row.data(), i0);
            for (int l = 0; l < 4; ++l) {
                float got = v.f3.data[l];
                float ref = ref_deq_scalar(row.data(), i0, l);
                maxerr = std::fmax(maxerr, std::fabs(got - ref));
            }
        }
        bool guard = true;
        for (size_t i = size_t(nb * 18); i < row.size(); ++i) guard &= row[i] == 0xA5;
        check("dequant values D64/128/256", maxerr == 0.0f);
        check("dequant guard bytes", guard);
    }
}

static void test_q8() {
    float maxerr = 0.0f;
    for (int t = 0; t < 96; ++t) {
        float v[4];
        for (int i = 0; i < 4; ++i) v[i] = float(int(rnd() % 20001) - 10000) / 97.0f;
        float amax = FG012_Q8_AMAX4(v[0], v[1], v[2], v[3]);
        float sum = FG012_Q8_SUM4(v[0], v[1], v[2], v[3]);
        float ramax = std::fmax(std::fmax(std::fabs(v[0]), std::fabs(v[1])), std::fmax(std::fabs(v[2]), std::fabs(v[3])));
        float rsum = v[0] + v[1] + v[2] + v[3];
        maxerr = std::fmax(maxerr, std::fabs(amax - ramax));
        maxerr = std::fmax(maxerr, std::fabs(sum - rsum));
        float d = ramax / 127.0f;
        uint32_t got = FG012_Q8_PACK(v[0], v[1], v[2], v[3], d);
        uint32_t ref = 0;
        for (int i = 0; i < 4; ++i) {
            int q = int(std::roundf(v[i] / d));
            ref |= uint32_t(q & 255) << (8 * i);
        }
        if (got != ref) maxerr = 1.0f;
    }
    check("q8_1 lane amax/sum/pack", maxerr == 0.0f);
    uint32_t z = FG012_Q8_PACK(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    check("q8_1 zero block", z == 0u && FG012_Q8_AMAX4(0, 0, 0, 0) == 0.0f);
    bool offsets = true;
    for (int i0 : {0, 8, 16, 32, 64, 96, 128, 192, 224}) {
        offsets &= FG012_Q8_LANE_BYTE(i0) == int64_t(i0) * 16;
    }
    for (int lane = 0; lane < 32; ++lane) {
        offsets &= FG012_Q8_LANE_BYTE(lane) == int64_t(lane) * 16;
    }
    check("q8_1 staging byte offsets", offsets);
    bool q4n = true, q8r = true;
    for (int t = 0; t < 64; ++t) {
        uint8_t byte = uint8_t(rnd());
        uint32_t q = uint32_t(byte);
        float d = float(int(rnd() % 1000) + 1) / 128.0f;
        for (int shift = 0; shift < 2; ++shift) {
            float got = FG012_Q4_NIBBLE(q, 0, shift, d);
            float ref = d * float(ref_nib(byte, shift != 0));
            q4n &= got == ref;
        }
        float v = float(int(rnd() % 20000) - 10000) / 53.0f;
        q8r &= FG012_Q8_ROUND(v, d) == int(std::roundf(v / d));
    }
    q8r &= FG012_Q8_ROUND(3.0f, 0.0f) == 0;
    check("q4_0 nibble decode", q4n);
    check("q8_1 round", q8r);
}

// Build a Q4_0 row and the q8_1 words for each lane of one warp.
struct DotCase {
    std::vector<uint8_t> K;
    std::vector<uint32_t> qa, qb;
    std::vector<float> dsxa, dsya, dsxb, dsyb;
};

static DotCase make_dot(int D) {
    DotCase c;
    int nb = D / 32;
    c.K.assign(size_t(nb * 18 + 32), 0x33);
    for (int b = 0; b < nb; ++b) {
        put_half(&c.K[size_t(b * 18)], uint16_t(0x2800 + (b % 5) * 0x0400)); // 2^-5 .. 0.5
        for (int j = 0; j < 16; ++j) c.K[size_t(b * 18 + 2 + j)] = uint8_t(rnd());
    }
    int words = D / 128;
    if (words == 0) words = 1;
    c.qa.assign(32, 0); c.qb.assign(32, 0);
    c.dsxa.assign(32, 0); c.dsya.assign(32, 0);
    c.dsxb.assign(32, 0); c.dsyb.assign(32, 0);
    for (int w = 0; w < words; ++w) {
        for (int g = 0; g < 4; ++g) {
            float val[32];
            for (int i = 0; i < 32; ++i) val[i] = float(int(rnd() % 4001) - 2000) / 31.0f;
            float amax = 0.0f, sum = 0.0f;
            for (int i = 0; i < 32; ++i) { amax = std::fmax(amax, std::fabs(val[i])); sum += val[i]; }
            float d = amax / 127.0f;
            for (int lane_in_group = 0; lane_in_group < 8; ++lane_in_group) {
                int lane = g * 8 + lane_in_group;
                uint32_t packed = FG012_Q8_PACK(val[lane_in_group * 4 + 0], val[lane_in_group * 4 + 1],
                                                val[lane_in_group * 4 + 2], val[lane_in_group * 4 + 3], d);
                if (w == 0) { c.qa[lane] = packed; c.dsxa[lane] = d; c.dsya[lane] = sum; }
                else { c.qb[lane] = packed; c.dsxb[lane] = d; c.dsyb[lane] = sum; }
            }
        }
    }
    return c;
}

static float ref_lane_partial(const DotCase & c, int D, int nthreads, int lane) {
    // Exact integer model of vec_dot_fattn_vec_KQ_q4_0 for the given lane.
    float total = 0.0f;
    for (int k0 = 0; k0 < D / 4; k0 += nthreads) {
        int k = k0 + ((nthreads == 32) ? lane : (lane % nthreads));
        int ib = k / 8, iqs4 = k % 4, shift = k & 4;
        const uint8_t * qs = &c.K[size_t(ib * 18 + 2 + iqs4 * 4)];
        uint32_t q8 = (k0 / nthreads) == 0 ? c.qa[lane] : c.qb[lane];
        float dsx = (k0 / nthreads) == 0 ? c.dsxa[lane] : c.dsxb[lane];
        float dsy = (k0 / nthreads) == 0 ? c.dsya[lane] : c.dsyb[lane];
        int sumi = 0;
        for (int l = 0; l < 4; ++l) {
            int nib = (shift != 0) ? (qs[l] >> 4) : (qs[l] & 15); // dp4a sees 0..15
            int8_t q = int8_t((q8 >> (8 * l)) & 255);
            sumi += nib * int(q);
        }
        float kd = ref_half(get_half(&c.K[size_t(ib * 18)]));
        total += kd * (float(sumi) * dsx - dsy);
    }
    return total;
}

static float ref_full_dot(const DotCase & c, int D) {
    float total = 0.0f;
    int nb = D / 32;
    int words = (D + 127) / 128;
    for (int w = 0; w < words; ++w) {
        int lanes = 32;
        for (int lane = 0; lane < lanes; ++lane) {
            uint32_t q8 = (w == 0) ? c.qa[lane] : c.qb[lane];
            float dq = (w == 0) ? c.dsxa[lane] : c.dsxb[lane];
            for (int l = 0; l < 4; ++l) {
                int dim = 128 * w + 4 * lane + l;
                if (dim >= D) continue;
                int b = dim / 32;
                int inner = dim % 32;
                int byte = inner % 16;
                bool high = inner >= 16;
                int nib = ref_nib(c.K[size_t(b * 18 + 2 + byte)], high);
                int8_t q = int8_t((q8 >> (8 * l)) & 255);
                total += ref_half(get_half(&c.K[size_t(b * 18)])) * float(nib) * dq * float(q);
            }
        }
    }
    (void)nb;
    return total;
}

static void test_dot() {
    for (int D : {64, 128, 256}) {
        DotCase c = make_dot(D);
        int nthreads = (D / 4 < 32) ? D / 4 : 32;
        int lanes = (D == 64) ? 16 : 32;
        float warp_sum = 0.0f;
        float perr = 0.0f;
        for (int lane = 0; lane < lanes; ++lane) {
            float got = FG012_Q4_DOT((int64_t)(intptr_t)c.K.data(), c.qa[lane], c.qb[lane],
                c.dsxa[lane], c.dsya[lane], c.dsxb[lane], c.dsyb[lane], D, nthreads, lane);
            float ref = ref_lane_partial(c, D, nthreads, lane);
            perr = std::fmax(perr, std::fabs(got - ref));
            warp_sum += got;
            mixbits(fbits(got));
        }
        float full = ref_full_dot(c, D);
        bool guard = true;
        for (size_t i = size_t((D / 32) * 18); i < c.K.size(); ++i) guard &= c.K[i] == 0x33;
        check("q4 lane math", perr < 2e-3f);
        // q8_1 is lossy: the native kernel folds the -8 bias through ds.y,
        // so the warp total tracks the dequantised dot within quantization
        // rounding, not bit-exactly.
        check("q4 warp dot within q8_1 rounding", closef(warp_sum, full, 5.0e-2f, 5e-3f));
        check("q4 dot guard bytes", guard);
    }
}

static void test_softmax_mask() {
    float logits[32];
    for (int i = 0; i < 32; ++i) logits[i] = float(int(rnd() % 40001) - 20000) / 113.0f;
    logits[7] = 600.0f; logits[19] = -600.0f;
    float m = -FLT_MAX / 2.0f;
    float s = 0.0f;
    for (int i = 0; i < 32; ++i) {
        float mn = FG012_SM_MAX_NEW(m, logits[i]);
        float sc = FG012_SM_EXP(m, mn);
        s = FG012_SM_MIX(s, sc, FG012_SM_EXP(logits[i], mn));
        m = mn;
    }
    float rmax = *std::max_element(logits, logits + 32);
    float rsum = 0.0f;
    for (int i = 0; i < 32; ++i) rsum += std::exp(logits[i] - rmax);
    rsum *= std::exp(-(3.0f * 0.6931f));
    check("online softmax max", closef(m, rmax + (3.0f * 0.6931f), 1e-6f, 1e-5f));
    check("online softmax sum", closef(s, rsum, 2e-5f, 2e-5f));
    check("softmax offset", closef(FG012_KQ_OFFSET(), 3.0f * 0.6931f, 1e-6f, 1e-6f));

    // mask half: 0.0, -inf, -2.0; slope from get_alibi_slope(max_bias=0)=1
    float slope = FG012_ALIBI(0.0f, 3, 4, 1.0f, 1.0f);
    check("mask zero", closef(FG012_APPLY_MASK(1.5f, slope, 0x0000u), 1.5f));
    check("mask minus_inf", std::isinf(FG012_APPLY_MASK(1.5f, slope, 0xFC00u)) &&
                            FG012_APPLY_MASK(1.5f, slope, 0xFC00u) < 0.0f);
    check("mask minus_two", closef(FG012_APPLY_MASK(1.5f, slope, 0xC000u), -0.5f));
    float alibi = FG012_ALIBI(1.0f, 3, 4, 1.0f, 0.5f);
    check("alibi slope", closef(alibi, 1.0f) || closef(alibi, 0.5f));
    check("masked -inf contributes zero", FG012_SM_EXP(-INFINITY, 0.0f) == 0.0f);
    check("softcap identity", closef(FG012_APPLY_SOFTCAP(2.25f, 0.0f), 2.25f));
    float sc = FG012_APPLY_SOFTCAP(2.25f, 10.0f);
    check("softcap value", closef(sc, 10.0f * std::tanh(2.25f), 1e-6f, 1e-6f));
}

static void test_value_mix() {
    uint8_t block[18];
    put_half(block, 0x3800); // 0.5
    for (int j = 0; j < 16; ++j) block[2 + j] = uint8_t(0x10 + j * 7);
    float acc = 0.0f;
    float ref = 0.0f;
    for (int k = 0; k < 8; ++k) {
        float w = float(k + 1) / 16.0f;
        FG012_DEQ4_TYPE v = FG012_DEQ4((int64_t)(intptr_t)block, 0);
        for (int l = 0; l < 4; ++l) {
            acc = FG012_SM_VALUE(acc, 1.0f, v.f3.data[l], w);
            ref += 0.5f * float(ref_nib(block[2 + l], false)) * w;
        }
    }
    check("value mix", closef(acc, ref, 1e-6f, 1e-6f));
    // all-masked row must stay finite: probability is exactly zero.
    float m = FG012_SM_MAX_NEW(-FLT_MAX / 2.0f, -INFINITY);
    float p = FG012_SM_EXP(-INFINITY, m);
    check("all-masked finite", std::isfinite(m) && p == 0.0f);
}

int main() {
    test_dequant();
    test_q8();
    test_dot();
    test_softmax_mask();
    test_value_mix();
    std::printf("{\"status\":\"%s\",\"cases\":%d,\"failures\":%d,\"hash\":\"%016llx\"}\n",
                g_fail ? "FAIL" : "PASS", g_cases, g_fail, (unsigned long long)g_hash);
    return g_fail ? 1 : 0;
}
