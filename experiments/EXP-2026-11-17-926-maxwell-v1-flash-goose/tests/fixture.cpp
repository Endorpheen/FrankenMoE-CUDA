// Differential gate for the Maxwell-V1-Flash decision module.
//
// Oracle: modeling_maxwell_moe.py (sha ca536c43) transcribed independently of
// the Goose source. Every check compares a randomized input against the same
// decision implemented here line by line from the HF code. Integer outputs must
// match exactly; float outputs within 1e-6 (they are the same float32 formula,
// only the summation order may differ).

#include "runtime-gen.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

static std::mt19937_64 rng(0x4d415857454c4cULL); // "MAXWELL"

static int g_fail = 0;
static int g_checks = 0;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d ", __FILE__, __LINE__);                    \
            std::printf(__VA_ARGS__);                                          \
            std::printf("\n");                                                 \
            ++g_fail;                                                          \
        }                                                                      \
    } while (0)

static float frand(float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng);
}

static int64_t irand(int64_t lo, int64_t hi) {
    std::uniform_int_distribution<int64_t> d(lo, hi);
    return d(rng);
}

static bool closef(float a, float b, float tol = 1e-6f) {
    if (std::isnan(a) || std::isnan(b)) return false;
    return std::fabs(a - b) <= tol * (1.0f + std::fabs(a) + std::fabs(b));
}

static sl_f32 S(std::vector<float> &v) { return sl_f32{v.data(), (int64_t)v.size()}; }
static sl_i64 I(std::vector<int64_t> &v) { return sl_i64{v.data(), (int64_t)v.size()}; }

// ---------------------------------------------------------------------------
// Oracle: geometry
// ---------------------------------------------------------------------------
static int64_t o_geom(int64_t pw, int64_t lw, int64_t blk, int64_t qc,
                      int64_t remote, int64_t *out) {
    if (blk <= 0) return -2;
    if (qc <= 0) return -3;
    if (pw <= 0) return -4;
    if (lw <= 0) return -5;
    if (pw % blk) return -6;
    if (lw % blk) return -7;
    if (remote <= 0) return -8;
    out[0] = pw / blk;
    out[1] = lw / blk;
    out[2] = qc;
    out[3] = remote;
    return 0;
}

// ---------------------------------------------------------------------------
// Oracle: router (sigmoid scores, bias only selects, normalize by raw scores)
// ---------------------------------------------------------------------------
static void o_router(const std::vector<float> &logits,
                     const std::vector<float> &bias, int64_t k,
                     std::vector<int64_t> &idx, std::vector<float> &w) {
    int64_t n = (int64_t)logits.size();
    std::vector<float> s(n);
    for (int64_t i = 0; i < n; ++i) s[i] = 1.0f / (1.0f + std::exp(-logits[i]));
    std::vector<int64_t> order(n);
    for (int64_t i = 0; i < n; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int64_t a, int64_t b) {
        float va = s[a] + bias[a], vb = s[b] + bias[b];
        if (va != vb) return va > vb;
        return a < b;
    });
    idx.assign(order.begin(), order.begin() + k);
    float sum = 0.0f;
    for (int64_t i = 0; i < k; ++i) sum += s[idx[i]];
    w.resize(k);
    for (int64_t i = 0; i < k; ++i) w[i] = s[idx[i]] / sum;
}

// ---------------------------------------------------------------------------
// Oracle: sinkhorn (softmax row, col-normalize, then row/col iterations)
// ---------------------------------------------------------------------------
static void o_sinkhorn(const std::vector<float> &logits, int64_t n, int64_t iters,
                       float eps, std::vector<float> &out) {
    int64_t nn = n * n;
    out.assign(nn, 0.0f);
    for (int64_t r = 0; r < n; ++r) {
        float m = logits[r * n];
        for (int64_t c = 1; c < n; ++c) m = std::max(m, logits[r * n + c]);
        float sum = 0.0f;
        for (int64_t c = 0; c < n; ++c) sum += std::exp(logits[r * n + c] - m);
        for (int64_t c = 0; c < n; ++c) out[r * n + c] = std::exp(logits[r * n + c] - m) / sum + eps;
    }
    for (int64_t c = 0; c < n; ++c) {
        float s = 0.0f;
        for (int64_t r = 0; r < n; ++r) s += out[r * n + c];
        for (int64_t r = 0; r < n; ++r) out[r * n + c] /= (s + eps);
    }
    for (int64_t it = 1; it < iters; ++it) {
        for (int64_t r = 0; r < n; ++r) {
            float s = 0.0f;
            for (int64_t c = 0; c < n; ++c) s += out[r * n + c];
            for (int64_t c = 0; c < n; ++c) out[r * n + c] /= (s + eps);
        }
        for (int64_t c = 0; c < n; ++c) {
            float s = 0.0f;
            for (int64_t r = 0; r < n; ++r) s += out[r * n + c];
            for (int64_t r = 0; r < n; ++r) out[r * n + c] /= (s + eps);
        }
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
static void test_geom() {
    for (int t = 0; t < 200; ++t) {
        int64_t blk = 1 << irand(5, 8);            // 32..256
        int64_t pw = blk * irand(1, 16);
        int64_t lw = blk * irand(1, 16);
        int64_t qc = irand(1, 128);
        int64_t remote = irand(1, 32);
        int64_t out[4] = {0};
        int64_t rc = mx_geom(pw, lw, blk, qc, remote, sl_i64{out, 4});
        int64_t ref[4] = {0};
        int64_t rr = o_geom(pw, lw, blk, qc, remote, ref);
        CHECK(rc == rr && std::memcmp(out, ref, sizeof(out)) == 0,
              "geom mismatch rc=%lld ref=%lld", (long long)rc, (long long)rr);
    }
    // Refusals, matching the native __init__ checks.
    int64_t out[4] = {0};
    CHECK(mx_geom(1024, 1024, 0, 64, 16, sl_i64{out, 4}) == -2, "blk<=0");
    CHECK(mx_geom(1000, 1024, 64, 64, 16, sl_i64{out, 4}) == -6, "prefix align");
    CHECK(mx_geom(1024, 1000, 64, 64, 16, sl_i64{out, 4}) == -7, "local align");
    CHECK(mx_geom(1024, 1024, 64, 64, 0, sl_i64{out, 4}) == -8, "remote<=0");
    CHECK(mx_geom(1024, 1024, 64, 64, 16, sl_i64{out, 3}) == -1, "short out");
}

static void test_selected_count() {
    CHECK(mx_selected_count(16, 4) == 4, "clamp high");
    CHECK(mx_selected_count(0, 8) == 1, "clamp low");
    CHECK(mx_selected_count(-5, 8) == 1, "negative");
    CHECK(mx_selected_count(16, 0) == 0, "no blocks");
    CHECK(mx_selected_count(999, 1000) == 999, "inside");
}

static void test_router() {
    for (int t = 0; t < 120; ++t) {
        int64_t n = irand(8, 72);
        int64_t k = irand(1, std::min<int64_t>(n, 16));
        std::vector<float> logits(n), bias(n);
        for (auto &x : logits) x = frand(-24.0f, 24.0f);
        for (auto &x : bias) x = frand(-0.01f, 0.01f);
        std::vector<int64_t> idx(k, -1), ref_idx;
        std::vector<float> w(k, 0.0f), ref_w;
        int64_t rc = mx_router(S(logits), S(bias), k, I(idx), S(w));
        CHECK(rc == 0, "router rc=%lld", (long long)rc);
        o_router(logits, bias, k, ref_idx, ref_w);
        bool ok = true;
        for (int64_t i = 0; i < k; ++i) {
            ok = ok && idx[i] == ref_idx[i] && closef(w[i], ref_w[i], 1e-5f);
        }
        CHECK(ok, "router mismatch n=%lld k=%lld", (long long)n, (long long)k);
        float sum = 0.0f;
        for (float x : w) sum += x;
        CHECK(closef(sum, 1.0f, 1e-4f), "router weights sum=%f", sum);
    }
    // Refusals.
    std::vector<float> l(4, 0.0f), b3(3, 0.0f), w(2, 0.0f);
    std::vector<int64_t> i2(2, 0);
    CHECK(mx_router(S(l), S(b3), 2, I(i2), S(w)) == -2, "bias len");
    CHECK(mx_router(S(l), S(l), 5, I(i2), S(w)) == -3, "topk>n");
    CHECK(mx_router(S(l), S(l), 2, I(i2), S(w)) == 0, "ok");
}

static void test_sinkhorn() {
    for (int t = 0; t < 100; ++t) {
        int64_t n = irand(2, 6);
        int64_t iters = irand(1, 25);
        float eps = (t % 2) ? 1e-6f : 0.0f;
        std::vector<float> logits(n * n), out(n * n, 0.0f), ref;
        for (auto &x : logits) x = frand(-8.0f, 8.0f);
        int64_t rc = mx_sinkhorn(S(logits), n, iters, eps, S(out));
        CHECK(rc == 0, "sinkhorn rc=%lld", (long long)rc);
        o_sinkhorn(logits, n, iters, eps, ref);
        bool ok = true;
        for (int64_t i = 0; i < n * n; ++i) ok = ok && closef(out[i], ref[i], 1e-5f);
        CHECK(ok, "sinkhorn mismatch n=%lld iters=%lld", (long long)n, (long long)iters);
        // The last normalisation is column-wise, so columns sum to one.
        for (int64_t c = 0; c < n; ++c) {
            float s = 0.0f;
            for (int64_t r = 0; r < n; ++r) s += out[r * n + c];
            CHECK(std::fabs(s - 1.0f) < 5e-2f, "sinkhorn col %lld = %f", (long long)c, s);
        }
    }
    std::vector<float> l(9, 0.0f), o(9, 0.0f);
    CHECK(mx_sinkhorn(S(l), 3, 0, 1e-6f, S(o)) == -1, "iters<=0");
    CHECK(mx_sinkhorn(S(l), 4, 20, 1e-6f, S(o)) == -2, "short buffers");
}

static void test_hc() {
    for (int t = 0; t < 80; ++t) {
        int64_t n = irand(2, 6);
        int64_t nC = n * irand(8, 32);
        int64_t mix = n * n + 2 * n;
        std::vector<float> x(nC);
        for (auto &v : x) v = frand(-4.0f, 4.0f);
        float eps = 1e-6f;
        float xn = mx_hc_xnorm(S(x), eps);
        float sum = 0.0f;
        for (float v : x) sum += v * v;
        float ref = 1.0f / (std::sqrt(sum) / std::sqrt((float)nC) + eps);
        CHECK(closef(xn, ref, 1e-5f), "hc_xnorm %f vs %f", xn, ref);

        std::vector<float> proj(mix), alpha(mix), bias(mix), pre(n), post(n), rl(n * n);
        std::vector<float> rpre(n), rpost(n), rrl(n * n);
        for (auto &v : proj) v = frand(-2.0f, 2.0f);
        for (auto &v : alpha) v = frand(-2.0f, 2.0f);
        for (auto &v : bias) v = frand(-2.0f, 2.0f);
        int64_t rc = mx_hc_mix(xn, S(proj), S(alpha), S(bias), n, eps,
                               S(pre), S(post), S(rl));
        CHECK(rc == 0, "hc_mix rc=%lld", (long long)rc);
        for (int64_t i = 0; i < n; ++i) {
            float hp = xn * proj[i] * alpha[i] + bias[i];
            rpre[i] = 1.0f / (1.0f + std::exp(-hp)) + eps;
            float hq = xn * proj[n + i] * alpha[n + i] + bias[n + i];
            rpost[i] = (1.0f / (1.0f + std::exp(-hq))) * 2.0f;
        }
        for (int64_t i = 0; i < n * n; ++i)
            rrl[i] = xn * proj[2 * n + i] * alpha[2 * n + i] + bias[2 * n + i];
        bool ok = true;
        for (int64_t i = 0; i < n; ++i)
            ok = ok && closef(pre[i], rpre[i], 1e-5f) && closef(post[i], rpost[i], 1e-5f);
        for (int64_t i = 0; i < n * n; ++i) ok = ok && closef(rl[i], rrl[i], 1e-5f);
        CHECK(ok, "hc_mix mismatch n=%lld", (long long)n);

        std::vector<float> mixes(n), w(n), rw(n);
        for (auto &v : mixes) v = frand(-6.0f, 6.0f);
        float scale = frand(-1.0f, 2.0f), base = frand(-1.0f, 1.0f);
        rc = mx_hc_head(S(mixes), scale, base, eps, S(w));
        CHECK(rc == 0, "hc_head rc=%lld", (long long)rc);
        for (int64_t i = 0; i < n; ++i)
            rw[i] = 1.0f / (1.0f + std::exp(-(mixes[i] * scale + base))) + eps;
        for (int64_t i = 0; i < n; ++i) CHECK(closef(w[i], rw[i], 1e-5f), "hc_head %lld", (long long)i);
    }
    // The h_pre eps floor is visible only where sigmoid underflows: hp = -30.
    {
        int64_t n = 2, mix = n * n + 2 * n;
        std::vector<float> proj(mix, 1.0f), alpha(mix, 1.0f), bias(mix, 0.0f);
        proj[0] = -30.0f;
        std::vector<float> pre(n, 0.0f), post(n, 0.0f), rl(n * n, 0.0f);
        CHECK(mx_hc_mix(1.0f, S(proj), S(alpha), S(bias), n, 1e-6f,
                        S(pre), S(post), S(rl)) == 0, "hc_mix underflow rc");
        float want = 1.0f / (1.0f + std::exp(30.0f)) + 1e-6f;
        CHECK(std::fabs(pre[0] - want) < 1e-9f, "hc pre eps floor: %.9g vs %.9g",
              (double)pre[0], (double)want);
    }
}

static void test_route() {
    for (int t = 0; t < 80; ++t) {
        int64_t blk = 1 << irand(5, 7);
        int64_t n_blocks = irand(12, 64);
        int64_t chunk = irand(1, 8);
        int64_t groups = irand(1, 5);
        int64_t remote = irand(1, 20);
        int64_t prefix_blocks = irand(0, 6);
        int64_t local_blocks = irand(1, 8);
        std::vector<int64_t> qpos(chunk);
        for (int64_t q = 0; q < chunk; ++q) qpos[q] = irand(0, n_blocks * blk - 1);
        std::vector<float> scores(chunk * groups * n_blocks);
        for (int64_t q = 0; q < chunk; ++q) {
            for (int64_t g = 0; g < groups; ++g) {
                for (int64_t b = 0; b < n_blocks; ++b) {
                    float v = (irand(0, 3) == 0) ? -INFINITY : frand(-10.0f, 10.0f);
                    scores[(q * groups + g) * n_blocks + b] = v;
                }
            }
        }
        std::vector<int64_t> out(chunk * groups * remote, 777), ref(chunk * groups * remote, 777);
        int64_t rc = mx_route(S(scores), n_blocks, chunk, groups, remote,
                              prefix_blocks, local_blocks, I(qpos), blk, I(out));
        CHECK(rc == 0, "route rc=%lld", (long long)rc);
        int64_t selected = std::min(std::max<int64_t>(remote, 1), n_blocks);
        for (int64_t q = 0; q < chunk; ++q) {
            int64_t cur = qpos[q] / blk;
            int64_t ls = std::max<int64_t>(cur - local_blocks + 1, 0) * blk;
            int64_t pe = prefix_blocks * blk;
            for (int64_t g = 0; g < groups; ++g) {
                std::vector<std::pair<float, int64_t>> cand;
                for (int64_t b = 0; b < n_blocks; ++b) {
                    int64_t bs = b * blk, be = bs + blk;
                    if (bs >= pe && be <= ls) {
                        float v = scores[(q * groups + g) * n_blocks + b];
                        if (!std::isinf(v) || v != -INFINITY) cand.push_back({v, b});
                    }
                }
                std::stable_sort(cand.begin(), cand.end(), [](auto &a, auto &b) {
                    if (a.first != b.first) return a.first > b.first;
                    return a.second < b.second;
                });
                for (int64_t k = 0; k < remote; ++k) {
                    ref[(q * groups + g) * remote + k] =
                        (k < (int64_t)cand.size() && k < selected) ? cand[k].second : -1;
                }
            }
        }
        CHECK(std::memcmp(out.data(), ref.data(), out.size() * sizeof(int64_t)) == 0,
              "route mismatch t=%d chunk=%lld groups=%lld remote=%lld", t,
              (long long)chunk, (long long)groups, (long long)remote);
    }
    std::vector<float> sc(4, 0.0f);
    std::vector<int64_t> qp(1, 0), o(1, 0);
    CHECK(mx_route(S(sc), 0, 1, 1, 1, 0, 1, I(qp), 128, I(o)) == -1, "no blocks");
    CHECK(mx_route(S(sc), 4, 1, 1, 0, 0, 1, I(qp), 128, I(o)) == -2, "remote<=0");
}

static void test_route_final() {
    for (int t = 0; t < 60; ++t) {
        int64_t blk = 1 << irand(5, 7);
        int64_t chunk = irand(1, 6);
        int64_t groups = irand(1, 4);
        int64_t remote_n = irand(1, 12);
        int64_t prefix_blocks = irand(0, 6);
        int64_t local_blocks = irand(1, 8);
        std::vector<int64_t> qpos(chunk), remote(chunk * groups * remote_n);
        for (int64_t q = 0; q < chunk; ++q) qpos[q] = irand(0, 40 * blk);
        for (auto &v : remote) v = irand(-1, 60);
        int64_t width = prefix_blocks + remote_n + local_blocks;
        std::vector<int64_t> out(chunk * groups * width, 999), ref(chunk * groups * width, 999);
        int64_t rc = mx_route_final(I(remote), chunk, groups, remote_n, prefix_blocks,
                                    local_blocks, I(qpos), blk, I(out));
        CHECK(rc == 0, "route_final rc=%lld", (long long)rc);
        for (int64_t q = 0; q < chunk; ++q) {
            int64_t cur = qpos[q] / blk;
            for (int64_t g = 0; g < groups; ++g) {
                int64_t base = (q * groups + g) * width;
                for (int64_t i = 0; i < prefix_blocks; ++i)
                    ref[base + i] = (i <= cur) ? i : -1;
                for (int64_t j = 0; j < remote_n; ++j)
                    ref[base + prefix_blocks + j] = remote[(q * groups + g) * remote_n + j];
                for (int64_t j = 0; j < local_blocks; ++j) {
                    int64_t b = cur + (1 - local_blocks + j);
                    ref[base + prefix_blocks + remote_n + j] = (b >= prefix_blocks) ? b : -1;
                }
            }
        }
        CHECK(std::memcmp(out.data(), ref.data(), out.size() * sizeof(int64_t)) == 0,
              "route_final mismatch t=%d", t);
    }
}

// Real Maxwell geometry: block 128, prefix 8, local 8, remote 16. For every
// query inside the 4096-token support window the route must cover every block
// up to the query (the dense-equivalence claim used by the builder).
static void test_route_real_geometry() {
    const int64_t blk = 128, prefix = 8, local = 8, remote = 16;
    const int64_t n_blocks = 32;
    for (int64_t q = 0; q < n_blocks; ++q) {
        std::vector<float> scores((size_t) n_blocks);
        for (auto &v : scores) v = frand(-5.0f, 5.0f);
        int64_t qpos = q*blk + irand(0, blk - 1);
        std::vector<int64_t> r((size_t) remote, -1), fin((size_t)(prefix + remote + local), -2);
        std::vector<int64_t> qp{qpos};
        CHECK(mx_route(S(scores), n_blocks, 1, 1, remote, prefix, local,
                       I(qp), blk, I(r)) == 0, "real route rc");
        CHECK(mx_route_final(I(r), 1, 1, remote, prefix, local, I(qp), blk, I(fin)) == 0,
              "real route_final rc");
        std::vector<int> seen(n_blocks, 0);
        for (int64_t b : fin) if (b >= 0 && b < n_blocks) seen[b] = 1;
        int missing = 0;
        for (int64_t b = 0; b <= q; ++b) if (!seen[b]) missing++;
        CHECK(missing == 0, "real geometry q=%lld missing %d blocks", (long long)q, missing);
    }
}

// Route bias: prefix 0..min(prefix-1,cur) plus local cur-local+1..cur with the
// >= prefix_blocks filter; everything else zero.
static void test_route_bias() {
    const int64_t blk = 128, prefix = 8, local = 8, n_blocks = 40;
    for (int64_t q = 0; q < 12; ++q) {
        std::vector<int64_t> qp{ q*blk + irand(0, blk-1) };
        std::vector<float> out((size_t) n_blocks, -1.0f);
        CHECK(mx_route_bias(n_blocks, 1, prefix, local, I(qp), blk, S(out)) == 0, "route_bias rc");
        int64_t cur = qp[0] / blk;
        for (int64_t b = 0; b < n_blocks; ++b) {
            bool forced = (b <= cur && b < prefix) ||
                          (b <= cur && b > cur - local && b >= prefix);
            CHECK((out[b] > 1.0e29) == forced,
                  "route_bias q=%lld b=%lld got=%f forced=%d", (long long)q, (long long)b,
                  (double)out[b], forced ? 1 : 0);
        }
    }
}

static void test_cfg_check() {
    std::vector<int64_t> o(4, -1);
    // real Maxwell: 2048 embd, 32/4 heads x128, indexer 4x128, 45 layers, 3
    // dense, block 128, prefix 1024, local 1024, 16 remote, ff 512/1024
    CHECK(mx_cfg_check(2048, 32, 4, 128, 4, 128, 45, 3, 128, 1024, 1024, 16, 512, 1024, I(o)) == 0,
          "cfg real");
    CHECK(o[0] == 8 && o[1] == 8 && o[2] == 16 && o[3] == 24, "cfg derived");
    CHECK(mx_cfg_check(2048, 32, 4, 128, 2, 128, 45, 3, 128, 1024, 1024, 16, 512, 1024, I(o)) == -4, "idx heads");
    CHECK(mx_cfg_check(2048, 32, 4, 128, 4, 64, 45, 3, 128, 1024, 1024, 16, 512, 1024, I(o)) == -5, "idx hd");
    CHECK(mx_cfg_check(2048, 32, 4, 128, 4, 128, 45, 46, 128, 1024, 1024, 16, 512, 1024, I(o)) == -6, "lead");
    CHECK(mx_cfg_check(2048, 32, 4, 128, 4, 128, 45, 3, 100, 1024, 1024, 16, 512, 1024, I(o)) == -7, "align");
    CHECK(mx_cfg_check(2048, 32, 4, 128, 4, 128, 45, 3, 128, 1024, 1024, 0, 512, 1024, I(o)) == -7, "remote");
    CHECK(mx_cfg_check(2048, 32, 6, 128, 6, 128, 45, 3, 128, 1024, 1024, 16, 512, 1024, I(o)) == -3, "gqa");
    CHECK(mx_cfg_check(2048, 32, 4, 128, 4, 128, 45, 3, 128, 1024, 1024, 16, 0, 1024, I(o)) == -8, "ff");
}

static void test_route_tokens() {
    {
        std::vector<int64_t> route{0, 5};
        std::vector<int64_t> pos(2*128, -9), ok(2*128, -9);
        CHECK(mx_route_tokens(I(route), 128, 600, I(pos), I(ok)) == 256, "tokens rc");
        CHECK(pos[0] == 0 && ok[0] == 1, "block 0 first");
        CHECK(pos[127] == 127 && ok[127] == 1, "block 0 last");
        CHECK(pos[128] == 640 && ok[128] == 0, "block 5 beyond kv");
        CHECK(pos[256-1] == 767 && ok[256-1] == 0, "last position");
    }
    {
        std::vector<int64_t> route{-1, 1};
        std::vector<int64_t> pos(2*8, -9), ok(2*8, -9);
        CHECK(mx_route_tokens(I(route), 8, 100, I(pos), I(ok)) == 16, "pad rc");
        CHECK(ok[0] == 0 && ok[7] == 0, "pad block invalid");
        CHECK(pos[8] == 8 && ok[8] == 1, "second block valid");
    }
    {
        std::vector<int64_t> route{0};
        std::vector<int64_t> pos(4, 0), ok(4, 0);
        CHECK(mx_route_tokens(I(route), 0, 100, I(pos), I(ok)) == -1, "zero block size");
        CHECK(mx_route_tokens(I(route), 8, 100, I(pos), I(ok)) == -2, "short out");
    }
}

static void test_reasoning_split() {
    auto u8s = [](const char *t) {
        return sl_u8{(uint8_t *)t, (int64_t)std::strlen(t)};
    };
    {
        std::vector<int64_t> o(4, -1);
        const char *t = "<think>abc\n</think>\nHello";
        CHECK(mx_split_reasoning(u8s(t), I(o)) == 0, "split ok rc");
        CHECK(o[0] == 7 && o[1] == 4, "think span %lld/%lld", (long long)o[0], (long long)o[1]);
        CHECK(o[2] == 20 && o[3] == 5, "answer span %lld/%lld", (long long)o[2], (long long)o[3]);
        CHECK(std::strncmp(t + o[2], "Hello", 5) == 0, "answer text");
    }
    {
        std::vector<int64_t> o(4, -1);
        CHECK(mx_split_reasoning(u8s("Plain answer"), I(o)) == 1, "no tags rc");
        CHECK(o[2] == 0 && o[3] == 12, "no tags answer");
    }
    {
        std::vector<int64_t> o(4, -1);
        CHECK(mx_split_reasoning(u8s("<think>abc"), I(o)) == -1, "unterminated");
        std::vector<int64_t> small(3, 0);
        CHECK(mx_split_reasoning(u8s("x"), I(small)) == -2, "short out");
    }
}

static void test_mtp() {
    for (int t = 0; t < 50; ++t) {
        int64_t n = irand(1, 40);
        int64_t pad = irand(0, 5);
        std::vector<int64_t> ids(n), out(n, -7), ref(n, -7);
        for (auto &v : ids) v = irand(0, 200000);
        CHECK(mx_mtp_roll(I(ids), pad, I(out)) == 0, "mtp rc");
        for (int64_t i = 0; i < n - 1; ++i) ref[i] = ids[i + 1];
        ref[n - 1] = pad;
        CHECK(std::memcmp(out.data(), ref.data(), n * sizeof(int64_t)) == 0, "mtp mismatch");
    }
}

int main() {
    goose_init(0, nullptr);
    test_geom();
    test_selected_count();
    test_router();
    test_sinkhorn();
    test_hc();
    test_route();
    test_route_final();
    test_mtp();
    test_route_real_geometry();
    test_route_bias();
    test_cfg_check();
    test_route_tokens();
    test_reasoning_split();
    std::printf("checks=%d failures=%d\n", g_checks, g_fail);
    if (g_fail == 0) std::printf("MAXWELL-DECISIONS OK\n");
    return g_fail == 0 ? 0 : 1;
}
