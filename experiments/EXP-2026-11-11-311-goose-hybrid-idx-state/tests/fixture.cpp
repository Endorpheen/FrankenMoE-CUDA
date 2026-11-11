// Differential gate for the Goose hybrid-idx state ordering.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg311dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg311k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg311k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg311k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg311k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg311k_fail(int64_t c) { std::fprintf(stderr, "FG311K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 10);

static int64_t call(int op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t n,
                    std::vector<int64_t> * arr, std::vector<int64_t> * arr2) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = a; g_packet[0][3] = b; g_packet[0][4] = c; g_packet[0][5] = d;
    g_packet[0][6] = n;
    g_packet[0][7] = arr ? (int64_t)(uintptr_t)arr->data() : 0;
    g_packet[0][8] = arr2 ? (int64_t)(uintptr_t)arr2->data() : 0;
    std::fill(out.begin(), out.end(), -99);
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg311dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(311011);

    // write order: base, then indexer as a suffix
    for (int trial = 0; trial < 200; ++trial) {
        const bool has_idx = (rng() & 1) != 0;
        const int64_t got = call(0, has_idx ? 1 : 0, 0, 0, 0, 0, nullptr, nullptr);
        check(got == (has_idx ? 2 : 1), "write section count", trial);
        check(out[0] == 0, "base section first", trial);
        if (has_idx) check(out[1] == 1, "indexer section last", trial);
    }

    // read order and the failure transaction
    for (int trial = 0; trial < 4000; ++trial) {
        const bool attn_fail = (rng() % 9) == 0;
        const bool recr_fail = (rng() % 9) == 0;
        const bool idx_fail  = (rng() % 9) == 0;
        const bool has_idx   = (rng() & 1) != 0;
        std::vector<int64_t> sinfos(1, 0);
        sinfos[0] = (rng() & 1) ? 1 : 0;            // whether the indexer returned a layout

        std::vector<int64_t> want;
        bool failed = false;
        if (attn_fail) failed = true;
        else {
            want.push_back(0);
            if (recr_fail) failed = true;
            else {
                want.push_back(1);
                if (has_idx) {
                    if (idx_fail) failed = true;
                    else {
                        want.push_back(2);
                        if (sinfos[0] != 0) want.push_back(3);
                    }
                }
            }
        }
        if (failed) want.push_back(4);

        const int64_t got = call(1, attn_fail ? 1 : 0, recr_fail ? 1 : 0, idx_fail ? 1 : 0,
                                 has_idx ? 1 : 0, 0, &sinfos, nullptr);
        check(got == (failed ? -1 : (int64_t)want.size()), "read result", trial);
        for (size_t i = 0; i < want.size(); ++i) check(out[i] == want[i], "read event order", trial);
    }

    // drop
    for (int trial = 0; trial < 1000; ++trial) {
        const int64_t seq = (rng() % 5) - 1;
        const bool has_idx = (rng() & 1) != 0;
        std::vector<int64_t> want;
        if (seq < 0) want.push_back(7);   // clear-all marker
        else {
            want.push_back(0);
            want.push_back(1);
            if (has_idx) { want.push_back(2); want.push_back(3); }
        }
        const int64_t got = call(2, seq, has_idx ? 1 : 0, 0, 0, 0, nullptr, nullptr);
        check(got == (int64_t)want.size(), "drop count", trial);
        for (size_t i = 0; i < want.size(); ++i) check(out[i] == want[i], "drop order", trial);
    }

    // ubatch stream counts
    for (int trial = 0; trial < 1000; ++trial) {
        const int n = 1 + (int)(rng() % 6);
        std::vector<int64_t> s0(n), s1(n);
        for (int i = 0; i < n; ++i) {
            s0[i] = (int64_t)(rng() % 8);
            s1[i] = s0[i] + (int64_t)(rng() % 5);
        }
        const int64_t got = call(3, 0, 0, 0, 0, n, &s0, &s1);
        check(got == n, "ns count", trial);
        for (int i = 0; i < n; ++i) check(out[i] == s1[i] - s0[i] + 1, "ns value", trial);
    }

    std::printf("FG311K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
