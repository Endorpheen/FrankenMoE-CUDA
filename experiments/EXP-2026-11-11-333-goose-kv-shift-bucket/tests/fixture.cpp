// Differential gate for the Goose shift input, rotation copy and buckets.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>

extern "C" uint8_t fg333dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg333k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg333k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg333k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg333k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg333k_fail(int64_t c) { std::fprintf(stderr, "FG333K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 16), cnt(1);

int main(void) {
    std::mt19937 rng(333011);

    // op 0: shift input
    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        std::vector<int64_t> sizes(n_stream), pos, shift;
        for (int64_t s = 0; s < n_stream; ++s) {
            sizes[s] = 1 + (int64_t)(rng() % 6);
            for (int64_t i = 0; i < sizes[s]; ++i) {
                pos.push_back((rng() % 4 == 0) ? -1 : (int64_t)(rng() % 50));
                shift.push_back((int64_t)(rng() % 21) - 10);
            }
        }
        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][0] = 0; g_packet[0][1] = (int64_t)(uintptr_t)out.data();
        g_packet[0][2] = n_stream; g_packet[0][3] = (int64_t)(uintptr_t)sizes.data();
        g_packet[0][4] = (int64_t)(uintptr_t)pos.data(); g_packet[0][5] = (int64_t)(uintptr_t)shift.data();
        g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
        g_packet[2][0] = 0; g_packet[2][1] = 0;
        if (!fg333dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        int64_t k = 0;
        int64_t budget = 0;   // total cells, for the count check
        for (int64_t s = 0; s < n_stream; ++s) {
            const int64_t sz = sizes[s];
            for (int64_t i = 0; i < sz; ++i, ++k) {
                const int64_t want = pos[budget + i] == -1 ? 0 : shift[budget + i];
                check(out[k*3] == 0, "shift kind", trial);
                check(out[k*3+1] == s*sz + i, "shift index", trial);
                check(out[k*3+2] == want, "shift value", trial);
            }
            budget += sz;
        }
        const int64_t off = budget;
        check(cnt[0] == off, "shift count", trial);
        check(g_packet[2][1] == off, "shift reported count", trial);
    }

    // op 1: rotation copy
    for (int trial = 0; trial < 500; ++trial) {
        const int64_t n_rot = 1 + (int64_t)(rng() % 40);
        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][0] = 1; g_packet[0][1] = (int64_t)(uintptr_t)out.data();
        const int64_t present = (rng() % 5) ? 1 : 0;
        const int64_t src = (rng() % 5) ? n_rot : n_rot + 1;
        g_packet[0][6] = present; g_packet[0][7] = n_rot; g_packet[0][8] = src;
        g_packet[1][0] = (int64_t)(uintptr_t)cnt.data();
        g_packet[2][0] = 0; g_packet[2][1] = 0;
        if (!fg333dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        if (!present || src != n_rot) {
            check(g_packet[2][1] == -1, "rot rejected", trial);
            check(cnt[0] == 0, "rot rejected emits nothing", trial);
        } else {
            check(g_packet[2][1] == n_rot*n_rot*4, "rot reported size", trial);
            check(cnt[0] == 1 && out[0] == 1 && out[1] == n_rot*n_rot*4, "rot copy size", trial);
        }
    }

        std::printf("FG333K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
