// Differential gate for the Goose batch initialisation policy.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg331dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg331k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg331k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg331k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg331k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg331k_fail(int64_t c) { std::fprintf(stderr, "FG331K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12), cnt(1);

struct Rec { int64_t kind, a, b; };

int main(void) {
    std::mt19937 rng(331011);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        const int64_t embd_all = rng() & 1;
        const int64_t n_ub = (int64_t)(rng() % 6);
        std::vector<int64_t> tok(n_ub + 1, 0), seqs(n_ub + 1, 0);
        for (int64_t i = 0; i < n_ub; ++i) {
            tok[i] = (rng() % 4 == 0) ? 0 : 1 + (int64_t)(rng() % 9);
            seqs[i] = 1 + (int64_t)(rng() % 4);
        }
        const int64_t n_tokens = (int64_t)(rng() % 40);
        const int64_t n_used = (rng() % 3) ? n_tokens : (int64_t)(rng() % (n_tokens + 1));
        const int64_t prepare_ok = (rng() % 4) ? 1 : 0;

        std::vector<Rec> want;
        int64_t want_ret = 0;
        const int64_t strategy = n_stream == 1 ? 0 : (embd_all ? 1 : 2);
        want.push_back({0, strategy, 0});
        int64_t taken = 0;
        for (int64_t i = 0; i < n_ub; ++i) {
            if (tok[i] == 0) break;
            want.push_back({1, tok[i], seqs[i]});
            ++taken;
        }
        if (n_used < n_tokens) {
            want.push_back({2, n_used, n_tokens});
            want.push_back({3, 0, 0});
            want_ret = 1;
        } else if (!prepare_ok) {
            want.push_back({3, 1, 0});
            want_ret = 1;
        } else {
            want.push_back({4, taken, 0});
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99);
        cnt[0] = 0;
        g_packet[0][0] = n_stream; g_packet[0][2] = embd_all;
        g_packet[0][3] = (int64_t)(uintptr_t)tok.data();
        g_packet[0][4] = (int64_t)(uintptr_t)seqs.data();
        g_packet[0][5] = n_ub;
        g_packet[0][6] = n_used; g_packet[0][7] = n_tokens; g_packet[0][8] = prepare_ok;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
        g_packet[3][0] = 0; g_packet[3][1] = 0;
        if (!fg331dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[3][1] == want_ret, "status", trial);
        check(cnt[0] == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)cnt[0]; ++i)
            check(out[i*3] == want[i].kind && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b,
                  "record", trial);
    }

    std::printf("FG331K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
