// Differential gate for the simple and sequential splits.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg351dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg351k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg351k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg351k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg351k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg351k_fail(int64_t c) { std::fprintf(stderr, "FG351K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12), cnt(1);
struct Rec { int64_t k, a, b; };

int main(void) {
    std::mt19937 rng(351011);

    for (int trial = 0; trial < 6000; ++trial) {
        const int64_t op = rng() & 1;
        const int64_t n_tokens = 1 + (int64_t)(rng() % 14);
        const int64_t n_ubatch = 1 + (int64_t)(rng() % 5);
        std::vector<int64_t> used((size_t)n_tokens), ss((size_t)n_tokens);
        for (int64_t i = 0; i < n_tokens; ++i) {
            used[(size_t)i] = (rng() % 3 == 0);
            int64_t m = 0;
            for (int64_t q = 0; q < 4; ++q) if (rng() & 1) m |= (1LL << q);
            ss[(size_t)i] = m;
        }

        // reference
        std::vector<Rec> want;
        std::vector<int64_t> rused = used;
        int64_t ret = 0;
        int64_t cur = 0;
        while (cur < n_tokens && rused[(size_t)cur]) ++cur;
        if (cur >= n_tokens) {
            want.push_back({3, 0, 0});
            ret = -1;
        } else if (op == 0) {
            int64_t count = 0;
            while (true) {
                want.push_back({0, cur, 0});
                rused[(size_t)cur] = 1;
                ++count;
                ++cur;
                if (cur >= n_tokens) break;
                if (count >= n_ubatch) break;
            }
            want.push_back({1, count, 0});
            ret = count;
        } else {
            int64_t count = 0;
            int64_t cur_set = ss[(size_t)cur];
            while (true) {
                want.push_back({0, cur, 0});
                rused[(size_t)cur] = 1;
                ++count;
                if (count >= n_ubatch) break;
                do { ++cur; } while (cur < n_tokens && (rused[(size_t)cur] || ((cur_set & ss[(size_t)cur]) != ss[(size_t)cur])));
                if (cur >= n_tokens) break;
                cur_set = ss[(size_t)cur];
            }
            want.push_back({1, count, 0});
            ret = count;
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][0] = op; g_packet[0][1] = n_tokens;
        g_packet[0][2] = (int64_t)(uintptr_t)used.data();
        g_packet[0][3] = (int64_t)(uintptr_t)ss.data();
        g_packet[0][4] = n_ubatch;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
        if (!fg351dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[2][1] == ret, "return", trial);
        check(cnt[0] == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)cnt[0]; ++i)
            check(out[i*3] == want[i].k && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b, "record", trial);
        for (size_t i = 0; i < used.size(); ++i) check(used[i] == rused[i], "used state", trial);
    }

    std::printf("FG351K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
