// Differential gate for the Goose recurrent initialisation and preparation.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg337dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg337k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg337k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg337k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg337k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg337k_fail(int64_t c) { std::fprintf(stderr, "FG337K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12), cnt(1);
struct Rec { int64_t k, a, b; };

static int64_t call(int64_t op, int64_t embd, int64_t n_rs, std::vector<int64_t> & ubtok, int64_t n_ub,
                    int64_t n_used, int64_t n_tok, std::vector<int64_t> & findok, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = op; g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = embd; g_packet[0][3] = n_rs;
    g_packet[0][4] = (int64_t)(uintptr_t)ubtok.data(); g_packet[0][5] = n_ub;
    g_packet[0][6] = n_used; g_packet[0][7] = n_tok;
    g_packet[0][8] = (int64_t)(uintptr_t)findok.data(); g_packet[0][9] = (int64_t)findok.size();
    g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
    if (!fg337dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

int main(void) {
    std::mt19937 rng(337011);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t embd = rng() & 1, n_rs = (int64_t)(rng() % 3);
        const int64_t n_ub = (int64_t)(rng() % 5);
        std::vector<int64_t> ubtok(n_ub + 1, 0);
        for (int64_t i = 0; i < n_ub; ++i) ubtok[i] = (rng() % 4 == 0) ? 0 : 1 + (int64_t)(rng() % 8);
        // the drain stops at the first zero, so only the prefix matters
        int64_t drained = 0;
        while (drained < n_ub && ubtok[(size_t)drained]) ++drained;
        const int64_t n_tok = (int64_t)(rng() % 30);
        const int64_t n_used = (rng() % 3) ? n_tok : (int64_t)(rng() % (n_tok + 1));
        std::vector<int64_t> findok((size_t)drained);
        for (int64_t i = 0; i < drained; ++i) findok[(size_t)i] = (rng() % 5) ? 1 : 0;

        std::vector<Rec> want; int64_t wret = 0;
        const int64_t mode = embd ? 0 : 1;
        const int64_t window = (!embd && n_rs > 0) ? n_rs + 1 : 0;
        want.push_back({0, mode, 1});     // always sequential
        if (mode == 1) want.push_back({1, window, 0});
        for (int64_t i = 0; i < drained; ++i) want.push_back({2, ubtok[(size_t)i], 0});
        if (n_used < n_tok) { wret = 1; want.push_back({3, 0, 0}); }
        else {
            want.push_back({4, 0, 0});
            bool ok = true;
            for (int64_t i = 0; i < drained; ++i) {
                want.push_back({5, i, 0});
                if (!findok[(size_t)i]) { want.push_back({6, i, 0}); ok = false; break; }
            }
            want.push_back({7, 0, 0});   // restored either way
            if (!ok) wret = 1;
        }
        std::vector<Rec> got;
        check(call(0, embd, n_rs, ubtok, n_ub, n_used, n_tok, findok, got) == wret, "init status", trial);
        check(got.size() == want.size(), "init count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a && got[i].b == want[i].b, "init record", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        std::vector<int64_t> e, f; std::vector<Rec> got;
        check(call(1, 0, 0, e, 0, 0, 0, f, got) == 0, "update status", trial);
        check(got.size() == 1 && got[0].k == 8, "no-update context", trial);
    }

    std::printf("FG337K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
