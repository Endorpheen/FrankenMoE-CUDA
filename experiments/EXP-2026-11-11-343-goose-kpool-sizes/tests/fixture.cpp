// Differential gate for the Goose k-pool context sizes.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

extern "C" uint8_t fg343dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg343k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg343k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg343k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg343k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg343k_fail(int64_t c) { std::fprintf(stderr, "FG343K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 10), cnt(1);
struct Rec { int64_t k, a, b; };

static int64_t call(int64_t op, int64_t idx_size, int64_t kpool, int64_t nsm, int64_t lay, int64_t track,
                    int64_t npr, int64_t nnew, int64_t nnewg, std::vector<Rec> & recs) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
    g_packet[0][0] = op; g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = idx_size; g_packet[0][3] = kpool; g_packet[0][4] = nsm; g_packet[0][5] = lay;
    g_packet[0][6] = track; g_packet[0][7] = npr; g_packet[0][8] = nnew; g_packet[0][9] = nnewg;
    g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
    if (!fg343dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    recs.clear();
    for (int64_t i = 0; i < cnt[0]; ++i) recs.push_back({out[i*3], out[i*3+1], out[i*3+2]});
    return g_packet[2][1];
}

static int64_t pad64(int64_t n) { return std::max<int64_t>(64, ((n + 1) + 63) / 64 * 64); }

int main(void) {
    std::mt19937 rng(343011);

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t lay = (int64_t)(rng() % 500);
        std::vector<Rec> got;
        check(call(0, 0, 1, 1, lay, 1, 0, 0, 0, got) == lay, "sizes ret", trial);
        check(got.size() == 1 && got[0].k == 0 && got[0].a == 0 && got[0].b == lay, "sizes record", trial);
    }

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t kpool = 1 + (int64_t)(rng() % 8);
        const bool empty_case = rng() % 8 == 0;   // nothing laid out and nothing cached: the bound must still be one
        const int64_t idx_size = empty_case ? 0 : (int64_t)(rng() % 300);
        const int64_t nsm = 1 + (int64_t)(rng() % 6);
        const int64_t lay = empty_case ? 0 : (int64_t)(rng() % 200);
        const int64_t track = (rng() % 6) ? 1 : 0;
        const int64_t worst = (idx_size / kpool) * nsm;
        const int64_t expect_npr = std::max(lay, worst);
        const int64_t expect_g = std::max<int64_t>(expect_npr, 1);

        std::vector<Rec> want, got;
        int64_t wret = expect_npr;
        if (!track) { want.push_back({2, 0, 0}); wret = -1; }
        else {
            want.push_back({1, worst, 0});
            want.push_back({0, 0, expect_npr});
            want.push_back({0, 1, expect_npr});
            want.push_back({0, 2, expect_g});
        }
        check(call(1, idx_size, kpool, nsm, lay, track, 0, 0, 0, got) == wret, "ctor ret", trial);
        check(got.size() == want.size(), "ctor count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i)
            check(got[i].k == want[i].k && got[i].a == want[i].a && got[i].b == want[i].b, "ctor record", trial);

        // accessors
        const int64_t npr = (int64_t)(rng() % 400);
        const int64_t nnewg = 1 + (int64_t)(rng() % 50);
        std::vector<Rec> got2;
        check(call(2, 0, 1, 1, 0, 1, npr, 0, nnewg, got2) == pad64(npr), "accessor ret", trial);
        check(got2.size() == 2 && got2[0].b == pad64(npr) && got2[1].b == nnewg, "accessor records", trial);
    }

    std::printf("FG343K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
