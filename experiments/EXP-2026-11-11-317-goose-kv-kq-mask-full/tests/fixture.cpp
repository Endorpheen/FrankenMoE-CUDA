// Differential gate for the Goose KV attention mask, full float path.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>

extern "C" uint8_t fg317dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg317k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg317k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg317k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg317k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg317k_fail(int64_t c) { std::fprintf(stderr, "FG317K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static const int64_t NEG_INF = -16777216;
static const int64_t POS_MAX = 2147483647;

static bool masked_swa(int64_t n_swa, int64_t t, int64_t p0, int64_t p1) {
    if (t == 0) return false;
    if (t == 1) return p1 - p0 >= n_swa;
    if (t == 2) return p0 < (p1 / n_swa) * n_swa;
    if (t == 3) { const int64_t h = n_swa / 2; const int64_t d = p1 - p0; return d < -h || d > h; }
    return false;
}

int main(void) {
    std::mt19937 rng(317011);

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t n_kv = 1 + (int64_t)(rng() % 14);
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        const int64_t n_tps = 1 + (int64_t)(rng() % 4);
        const int64_t n_tokens = n_stream * n_tps;
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 4);
        const int64_t n_swa = 1 + (int64_t)(rng() % 80);
        const int64_t swa_type = (int64_t)(rng() % 4);
        const bool causal = (rng() % 4) != 0;
        const bool swa_on = (rng() % 2) != 0;
        const bool alibi = (rng() % 3) == 0;
        const bool is_2d = (rng() % 3) == 0;
        const bool nc_full = (rng() % 3) == 0;

        std::vector<int64_t> cpos((size_t)n_stream*n_kv, -1), cseqw((size_t)n_stream*n_kv, 0);
        std::vector<int64_t> extx((size_t)n_stream*n_kv, 0), exty((size_t)n_stream*n_kv, 0);
        for (size_t c = 0; c < cpos.size(); ++c) {
            if (rng() % 5 == 0) continue;
            cpos[c] = (int64_t)(rng() % 200);
            for (int64_t s = 0; s < n_seq_max; ++s) if (rng() % 2) cseqw[c] |= (1ull << s);
            if (cseqw[c] == 0) cseqw[c] = 1ull << (rng() % n_seq_max);
            extx[c] = (int64_t)(rng() % 6);
            exty[c] = (int64_t)(rng() % 6);
        }
        std::vector<int64_t> s2s(n_seq_max), ub_seq((size_t)n_tokens*4, 0), pos(n_tokens);
        std::vector<int64_t> p1x(n_tokens, 0), p1y(n_tokens, 0);
        for (int64_t s = 0; s < n_seq_max; ++s) s2s[s] = (int64_t)(rng() % n_stream);
        for (int64_t i = 0; i < n_tokens; ++i) {
            const int64_t s = (int64_t)(rng() % n_seq_max);
            ub_seq[i*4] = s;
            std::vector<int64_t> cand;
            for (int64_t j = 0; j < n_kv; ++j) {
                const int64_t c = s2s[s]*n_kv + j;
                if (cpos[c] >= 0 && (cseqw[c] >> s & 1)) cand.push_back(cpos[c]);
            }
            pos[i] = cand.empty() ? (int64_t)(rng() % 200) : cand[(size_t)(rng() % cand.size())];
            p1x[i] = (int64_t)(rng() % 6);
            p1y[i] = (int64_t)(rng() % 6);
        }

        std::vector<int64_t> smin(n_seq_max, POS_MAX);
        for (int64_t i = 0; i < n_tokens; ++i) smin[ub_seq[i*4]] = std::min(smin[ub_seq[i*4]], pos[i]);

        std::vector<int64_t> want((size_t)n_tokens*n_kv, 0);
        for (int64_t i = 0; i < n_tokens; ++i) {
            const int64_t sid = ub_seq[i*4];
            const int64_t stream = s2s[sid];
            const int64_t p1 = pos[i];
            for (int64_t j = 0; j < n_kv; ++j) {
                const int64_t c = stream*n_kv + j;
                int64_t v = NEG_INF;
                if (cpos[c] >= 0 && ((cseqw[c] >> sid) & 1)) {
                    bool drop = false;
                    if (causal && cpos[c] > p1) drop = true;
                    if (!drop && causal && is_2d && cpos[c] == p1) {
                        const int64_t ex = extx[c], ey = exty[c];
                        if (ey > p1y[i]) drop = true;
                        else if (ey == p1y[i] && ex > p1x[i]) drop = true;
                    }
                    if (!drop && swa_on) {
                        const bool in_span = !causal && nc_full && cpos[c] >= smin[sid];
                        if (!in_span && masked_swa(n_swa, swa_type, cpos[c], p1)) drop = true;
                    }
                    if (!drop) v = alibi ? -std::abs(cpos[c] - p1) : 0;
                }
                want[i*n_kv + j] = v;
            }
        }

        static std::vector<int64_t> mask(1<<16), srct(64), counts(64), idxs(1<<16), sminv(64);
        std::fill(mask.begin(), mask.end(), -99);
        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        g_packet[0][1] = n_kv; g_packet[0][2] = n_stream; g_packet[0][3] = n_tps;
        g_packet[0][4] = n_tokens; g_packet[0][5] = n_seq_max; g_packet[0][6] = n_swa;
        g_packet[0][7] = swa_type; g_packet[0][8] = causal ? 1 : 0; g_packet[0][9] = swa_on ? 1 : 0;
        g_packet[0][10] = (int64_t)(uintptr_t)s2s.data();
        g_packet[0][11] = (int64_t)(uintptr_t)ub_seq.data();
        g_packet[0][12] = 4;
        g_packet[0][13] = (int64_t)(uintptr_t)pos.data();
        g_packet[1][0] = (int64_t)(uintptr_t)cpos.data();
        g_packet[1][1] = (int64_t)(uintptr_t)cseqw.data();
        g_packet[1][2] = 1;
        g_packet[1][3] = (int64_t)(uintptr_t)extx.data();
        g_packet[1][4] = (int64_t)(uintptr_t)exty.data();
        g_packet[1][5] = (int64_t)(uintptr_t)p1x.data();
        g_packet[1][6] = (int64_t)(uintptr_t)p1y.data();
        g_packet[1][7] = alibi ? 1 : 0;
        g_packet[1][8] = is_2d ? 1 : 0;
        g_packet[1][9] = nc_full ? 1 : 0;
        g_packet[2][0] = (int64_t)(uintptr_t)mask.data();
        g_packet[2][1] = (int64_t)(uintptr_t)srct.data();
        g_packet[2][2] = (int64_t)(uintptr_t)counts.data();
        g_packet[2][3] = (int64_t)(uintptr_t)idxs.data();
        g_packet[2][4] = (int64_t)(uintptr_t)sminv.data();
        g_packet[3][0] = 0; g_packet[3][1] = 0;
        if (!fg317dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        check(g_packet[3][1] == n_tokens, "token count", trial);
        for (size_t i = 0; i < want.size(); ++i) check(mask[i] == want[i], "mask value", trial);
    }

    std::printf("FG317K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
