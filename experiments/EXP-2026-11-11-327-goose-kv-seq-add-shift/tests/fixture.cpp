// Differential gate for the Goose sequence shift.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg327dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg327k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg327k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg327k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg327k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg327k_fail(int64_t c) { std::fprintf(stderr, "FG327K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);

struct Rec { int64_t kind, a, b; };

int main(void) {
    std::mt19937 rng(327011);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n = 1 + (int64_t)(rng() % 12);
        const int64_t other = (rng() % 8 == 0);
        const int64_t shift = (rng() % 6 == 0) ? 0 : (int64_t)(rng() % 21) - 10;
        int64_t p0 = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 20);
        int64_t p1 = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 20);

        std::vector<int64_t> pos(n), bits(n);
        for (int64_t i = 0; i < n; ++i) {
            pos[i] = (rng() % 5 == 0) ? -1 : (int64_t)(rng() % 20);
            bits[i] = (rng() & 1);
        }
        const int64_t head = (int64_t)(rng() % (n + 3));

        // reference
        std::vector<Rec> want;
        int64_t want_count = 0;
        if (!other && shift != 0) {
            int64_t a0 = p0 < 0 ? 0 : p0;
            int64_t a1 = p1 < 0 ? (int64_t)1 << 62 : p1;
            if (a0 != a1) {
                int64_t new_head = n;
                for (int64_t i = 0; i < n; ++i) {
                    const bool inside = pos[i] >= a0 && pos[i] < a1;
                    const bool has = bits[i] != 0;
                    if (!inside || !has) continue;
                    want.push_back({2, i, 0});
                    const int64_t np = pos[i] + shift;
                    want.push_back({0, i, np});
                    ++want_count;
                    if (np < 0) {
                        if (new_head == n) new_head = i;
                    } else {
                        want.push_back({3, i, 0});
                    }
                }
                want.push_back({1, new_head != n ? new_head : 0, 0});
            }
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99);
        cnt[0] = 0;
        g_packet[0][0] = other; g_packet[0][1] = n; g_packet[0][2] = 0;
        g_packet[0][3] = p0; g_packet[0][4] = p1; g_packet[0][5] = shift;
        g_packet[0][6] = (int64_t)(uintptr_t)pos.data();
        g_packet[0][7] = (int64_t)(uintptr_t)bits.data();
        g_packet[0][9] = (int64_t)(uintptr_t)&head;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
        g_packet[3][0] = 0; g_packet[3][1] = 0;
        if (!fg327dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[3][1] == want_count, "shift count", trial);
        check(cnt[0] == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)cnt[0]; ++i)
            check(out[i*3] == want[i].kind && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b,
                  "record", trial);
    }

    std::printf("FG327K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
