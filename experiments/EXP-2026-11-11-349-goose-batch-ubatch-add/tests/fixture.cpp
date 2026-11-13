// Differential gate for the ubatch assembly decisions.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

extern "C" uint8_t fg349dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg349k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg349k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg349k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg349k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg349k_fail(int64_t c) { std::fprintf(stderr, "FG349K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);
struct Rec { int64_t k, a, b; };

int main(void) {
    std::mt19937 rng(349011);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t n_batch = 1 + (int64_t)(rng() % 12);
        const int64_t n = 1 + (int64_t)(rng() % 8);
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 5);
        const int64_t n_embd = 4 + (int64_t)(rng() % 8);
        const int64_t n_pos = 1 + (int64_t)(rng() % 3);
        const int64_t has_embd_vec = (rng() % 3) ? 1 : 0;
        const int64_t has_state = rng() & 1;
        const int64_t has_token = rng() & 1;
        const int64_t has_embdv = rng() & 1;

        std::vector<int64_t> idxs((size_t)n), logits((size_t)n_batch), nseqid((size_t)n_batch), seqid((size_t)n_batch*4, 0), embd((size_t)n_batch, 0);
        for (int64_t i = 0; i < n; ++i) idxs[(size_t)i] = (int64_t)(rng() % n_batch);
        for (int64_t i = 0; i < n_batch; ++i) {
            logits[(size_t)i] = rng() & 1;
            nseqid[(size_t)i] = 1 + (int64_t)(rng() % 2);
            for (int64_t j = 0; j < nseqid[(size_t)i]; ++j) seqid[(size_t)(i*4+j)] = (int64_t)(rng() % n_seq_max);
            embd[(size_t)i] = rng() & 1;
        }

        std::vector<Rec> want; int64_t wret = 0;
        int64_t n_embd_rows = 0;
        if (has_embd_vec) for (int64_t i = 0; i < n; ++i) n_embd_rows += embd[(size_t)idxs[(size_t)i]];
        const bool mixed_batch = has_embd_vec != 0;
        const bool mixed = mixed_batch && n_embd_rows > 0 && n_embd_rows < n;
        const bool use_token = has_token && !(mixed_batch && n_embd_rows == n);
        const bool use_embd = has_embdv && !(mixed_batch && n_embd_rows == 0);
        const bool use_state = has_state != 0;

        want.push_back({0, 0, mixed});
        want.push_back({0, 1, use_token});
        want.push_back({0, 2, use_embd});
        want.push_back({0, 3, use_state});
        want.push_back({1, 0, use_embd ? n*n_embd : 0});
        want.push_back({1, 1, use_state ? n*n_embd : 0});
        want.push_back({1, 2, n*n_pos});
        int64_t o = 0;
        for (int64_t i = 0; i < n; ++i) {
            const int64_t idx = idxs[(size_t)i];
            want.push_back({2, i, nseqid[(size_t)idx]});
            want.push_back({6, i, logits[(size_t)idx]});
            if (logits[(size_t)idx]) { want.push_back({3, o, idx}); ++o; }
        }
        int64_t u = 0;
        for (int64_t s = 0; s < n_seq_max; ++s) {
            bool present = false;
            for (int64_t r = 0; r < n && !present; ++r) {
                const int64_t idx = idxs[(size_t)r];
                for (int64_t j = 0; j < nseqid[(size_t)idx]; ++j) if (seqid[(size_t)(idx*4+j)] == s) present = true;
            }
            if (present) { want.push_back({4, s, u}); want.push_back({5, s, u}); ++u; }
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][0] = n;
        g_packet[0][1] = (int64_t)(uintptr_t)idxs.data();
        g_packet[0][2] = (int64_t)(uintptr_t)logits.data();
        g_packet[0][3] = (int64_t)(uintptr_t)nseqid.data();
        g_packet[0][4] = (int64_t)(uintptr_t)seqid.data();
        g_packet[0][5] = n_seq_max;
        g_packet[0][6] = (int64_t)(uintptr_t)embd.data();
        g_packet[0][7] = n_embd;
        g_packet[0][8] = has_embd_vec; g_packet[0][9] = has_state;
        g_packet[0][10] = has_token; g_packet[0][11] = has_embdv;
        g_packet[0][12] = n_pos;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
        if (!fg349dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[2][1] == wret, "status", trial);
        check(cnt[0] == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)cnt[0]; ++i)
            check(out[i*3] == want[i].k && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b, "record", trial);
    }

    std::printf("FG349K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
