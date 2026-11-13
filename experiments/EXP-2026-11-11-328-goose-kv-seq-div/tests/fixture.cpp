// Differential gate for the Goose sequence division and position extremes.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg328dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg328k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg328k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg328k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg328k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg328k_fail(int64_t c) { std::fprintf(stderr, "FG328K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);

struct Rec { int64_t kind, a, b; };

static int64_t call(int op, int64_t other, int64_t n, int64_t p0, int64_t p1, int64_t d,
                    std::vector<int64_t> & pos, std::vector<int64_t> & bits,
                    int64_t other_min, int64_t other_max, int64_t & nrec) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99);
    cnt[0] = 0;
    g_packet[0][0] = op; g_packet[0][1] = other; g_packet[0][2] = n; g_packet[0][3] = 0;
    g_packet[0][4] = p0; g_packet[0][5] = p1; g_packet[0][6] = d;
    g_packet[0][7] = (int64_t)(uintptr_t)pos.data();
    g_packet[0][8] = (int64_t)(uintptr_t)bits.data();
    g_packet[0][9] = other_min; g_packet[0][10] = other_max;
    g_packet[1][0] = (int64_t)(uintptr_t)out.data();
    g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg328dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    nrec = cnt[0];
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(328011);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n = 1 + (int64_t)(rng() % 12);
        const int64_t other = (rng() % 8 == 0);
        int64_t d = (int64_t)(rng() % 7) - 1;
        if (d == 0) d = 1 + (int64_t)(rng() % 3);
        int64_t p0 = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 40);
        int64_t p1 = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 40);

        std::vector<int64_t> pos(n), bits(n);
        for (int64_t i = 0; i < n; ++i) {
            pos[i] = (rng() % 5 == 0) ? -1 : (int64_t)(rng() % 40);
            bits[i] = (rng() & 1);
        }

        std::vector<Rec> want;
        int64_t want_ret = 0;
        if (!other && d != 1) {
            const int64_t a0 = p0 < 0 ? 0 : p0;
            const int64_t a1 = p1 < 0 ? (int64_t)1 << 62 : p1;
            if (a0 != a1) {
                for (int64_t i = 0; i < n; ++i) {
                    if (pos[i] < a0 || pos[i] >= a1) continue;
                    if (!bits[i]) continue;
                    const int64_t np = pos[i] / d;
                    want.push_back({0, i, 0});
                    want.push_back({1, i, np});
                    want.push_back({3, i, pos[i] - np});
                    want.push_back({2, i, 0});
                    ++want_ret;
                }
            }
        }

        int64_t nrec = 0;
        const int64_t ret = call(0, other, n, p0, p1, d, pos, bits, 0, 0, nrec);
        check(ret == want_ret, "division count", trial);
        check(nrec == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)nrec; ++i)
            check(out[i*3] == want[i].kind && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b,
                  "record", trial);

        // extremes
        for (int op = 1; op <= 2; ++op) {
            int64_t want_e = -1;
            bool found = false;
            if (other) {
                want_e = op == 1 ? 777 : -888;
                found = true;
            } else {
                for (int64_t i = 0; i < n; ++i) {
                    if (pos[i] == -1 || !bits[i]) continue;
                    if (!found) { want_e = pos[i]; found = true; }
                    else if (op == 1) want_e = std::min(want_e, pos[i]);
                    else              want_e = std::max(want_e, pos[i]);
                }
            }
            int64_t nr2 = 0;
            check(call(op, other, n, 0, 0, 1, pos, bits, 777, -888, nr2) == want_e,
                  op == 1 ? "seq_pos_min" : "seq_pos_max", trial);
        }
    }

    std::printf("FG328K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
