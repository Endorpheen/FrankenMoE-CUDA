// Differential gate for the batch entry classification.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg352dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg352k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg352k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg352k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg352k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg352k_fail(int64_t c) { std::fprintf(stderr, "FG352K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 13), cnt(1);
struct Rec { int64_t k, a, b; };

int main(void) {
    std::mt19937 rng(352011);

    for (int trial = 0; trial < 6000; ++trial) {
        const int64_t n = 1 + (int64_t)(rng() % 6);
        const int64_t n_vocab = 4 + (int64_t)(rng() % 8);
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 4);
        const int64_t allow_mixed = rng() & 1;
        const int64_t batch_state = rng() & 1;

        std::vector<int64_t> is_tok((size_t)n), has_embd((size_t)n), has_state((size_t)n), id((size_t)n), nseqid((size_t)n), seqid((size_t)n * 4, 0);
        for (int64_t i = 0; i < n; ++i) {
            // deliberately allow "neither" and "both" and non-uniform state
            is_tok[(size_t)i] = (rng() % 5 == 0) ? 0 : 1;
            has_embd[(size_t)i] = rng() & 1;
            has_state[(size_t)i] = (rng() % 6 == 0) ? (1 - batch_state) : batch_state;
            id[(size_t)i] = (int64_t)(rng() % (n_vocab + 2)) - 1;
            nseqid[(size_t)i] = (int64_t)(rng() % 3);
            for (int64_t j = 0; j < nseqid[(size_t)i]; ++j) seqid[(size_t)(i*4+j)] = (int64_t)(rng() % (n_seq_max + 1));
        }

        // reference
        std::vector<Rec> want; int64_t wret = 0;
        const int64_t state_expected = has_state[0];
        int64_t n_tok_only = 0, n_embd_only = 0, n_both = 0;
        bool bad = false;
        for (int64_t i = 0; i < n && !bad; ++i) {
            const bool t = is_tok[(size_t)i] != 0, e = has_embd[(size_t)i] != 0;
            if (!t && !e) { wret = -1; bad = true; break; }
            if (has_state[(size_t)i] != state_expected) { wret = -2; bad = true; break; }
            if (t && !e) ++n_tok_only;
            if (e && !t) ++n_embd_only;
            if (t && e) ++n_both;
        }
        if (!bad) {
            want.push_back({0, 0, n_tok_only});
            want.push_back({0, 1, n_embd_only});
            want.push_back({0, 2, n_both});
            if (n_both > 0 && n_both != n) { wret = -3; }
            else {
                const bool mixed = n_tok_only > 0 && n_embd_only > 0;
                if (mixed && !allow_mixed) { wret = -4; }
                else {
                    const bool has_token_f = n_tok_only > 0 || n_both > 0;
                    const bool has_embd_f = n_embd_only > 0 || n_both > 0;
                    want.push_back({1, 0, mixed});
                    want.push_back({1, 1, has_token_f});
                    want.push_back({1, 2, has_embd_f});
                    want.push_back({1, 3, state_expected});
                    if (mixed)
                        for (int64_t i = 0; i < n; ++i) want.push_back({2, i, has_embd[(size_t)i]});
                    if (has_token_f) {
                        for (int64_t i = 0; i < n && wret == 0; ++i) {
                            const bool marked = mixed && has_embd[(size_t)i];
                            if (marked) { want.push_back({3, i, 0}); continue; }
                            const int64_t v = id[(size_t)i];
                            if (v < 0 || v >= n_vocab) { wret = -5; break; }
                            want.push_back({4, i, v});
                        }
                    }
                    if (wret == 0) {
                        for (int64_t i = 0; i < n && wret == 0; ++i)
                            for (int64_t j = 0; j < nseqid[(size_t)i]; ++j) {
                                const int64_t sid = seqid[(size_t)(i*4+j)];
                                if (sid < 0 || sid >= n_seq_max) { wret = -6; break; }
                                want.push_back({5, i, sid});
                            }
                    }
                }
            }
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][0] = n;
        g_packet[0][1] = (int64_t)(uintptr_t)is_tok.data();
        g_packet[0][2] = (int64_t)(uintptr_t)has_embd.data();
        g_packet[0][3] = (int64_t)(uintptr_t)has_state.data();
        g_packet[0][4] = (int64_t)(uintptr_t)id.data();
        g_packet[0][5] = n_vocab;
        g_packet[0][6] = (int64_t)(uintptr_t)nseqid.data();
        g_packet[0][7] = (int64_t)(uintptr_t)seqid.data();
        g_packet[0][8] = n_seq_max;
        g_packet[0][9] = allow_mixed;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
        if (!fg352dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[2][1] == wret, "status", trial);
        check(cnt[0] == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)cnt[0]; ++i)
            check(out[i*3] == want[i].k && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b, "record", trial);
    }

    std::printf("FG352K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
