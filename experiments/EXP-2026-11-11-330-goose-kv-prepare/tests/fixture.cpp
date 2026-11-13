// Differential gate for the Goose KV batch preparation transaction.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg330dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg330k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg330k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg330k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg330k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg330k_fail(int64_t c) { std::fprintf(stderr, "FG330K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);

int main(void) {
    std::mt19937 rng(330011);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_ub = 1 + (int64_t)(rng() % 5);
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        const int64_t hs = n_stream;
        std::vector<int64_t> ncells(n_ub), findok(n_ub), off(n_ub + 1, 0);
        for (int64_t u = 0; u < n_ub; ++u) {
            ncells[u] = 1 + (int64_t)(rng() % 4);
            findok[u] = (rng() % 5) ? 1 : 0;
            off[u+1] = off[u] + ncells[u];
        }
        (void)off[n_ub];
        std::vector<int64_t> heads((size_t)n_ub*n_stream);
        for (size_t i = 0; i < heads.size(); ++i) heads[i] = (int64_t)(rng() % 100);

        // reference
        std::vector<int64_t> w;   // triples
        int64_t want_ret = 0;
        int64_t done_ub = 0;
        bool failed = false;
        for (int64_t u = 0; u < n_ub; ++u) {
            if (!findok[u]) { failed = true; break; }
            w.push_back(0); w.push_back(u); w.push_back(0);
            for (int64_t c = 0; c < ncells[u]; ++c) { w.push_back(1); w.push_back(off[u]+c); w.push_back(0); }
            w.push_back(2); w.push_back(u); w.push_back(0);
            ++done_ub;
        }
        if (failed) {
            for (int64_t ru = done_ub - 1; ru >= 0; --ru) {
                for (int64_t c = 0; c < ncells[ru]; ++c) { w.push_back(3); w.push_back(off[ru]+c); w.push_back(0); }
                for (int64_t s = 0; s < n_stream; ++s) { w.push_back(4); w.push_back(s); w.push_back(heads[ru*hs+s]); }
            }
            want_ret = -1;
        } else {
            want_ret = done_ub;
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99);
        cnt[0] = 0;
        g_packet[0][0] = n_ub;
        g_packet[0][1] = (int64_t)(uintptr_t)ncells.data();
        g_packet[0][2] = (int64_t)(uintptr_t)findok.data();
        g_packet[0][6] = 0;
        g_packet[0][7] = (int64_t)(uintptr_t)off.data();
        g_packet[0][8] = (int64_t)(uintptr_t)heads.data();
        g_packet[0][9] = hs; g_packet[0][10] = n_stream;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
        g_packet[3][0] = 0; g_packet[3][1] = 0;
        if (!fg330dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[3][1] == want_ret, "return value", trial);
        check(cnt[0] == (int64_t)w.size()/3, "record count", trial);
        for (int64_t i = 0; i < cnt[0] && i < (int64_t)w.size()/3; ++i)
            check(out[i*3] == w[i*3] && out[i*3+1] == w[i*3+1] && out[i*3+2] == w[i*3+2],
                  "record", trial);
    }


    std::printf("FG330K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
