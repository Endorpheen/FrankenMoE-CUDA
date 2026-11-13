// Differential gate for the Goose hybrid-idx context stepping.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg313dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg313k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg313k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg313k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg313k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg313k_fail(int64_t c) { std::fprintf(stderr, "FG313K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 10);

static int64_t call(int op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t n, std::vector<int64_t> * arr) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    std::fill(out.begin(), out.end(), -99);
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = a; g_packet[0][3] = b; g_packet[0][4] = c; g_packet[0][5] = d;
    g_packet[0][6] = n;
    g_packet[0][7] = arr ? (int64_t)(uintptr_t)arr->data() : 0;
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg313dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(313011);

    for (int trial = 0; trial < 2000; ++trial) {
        const bool mem = rng() & 1, idx = rng() & 1;
        const int64_t kpool = (int64_t)(rng() % 3);
        const int64_t ns_len = (int64_t)(rng() % 3);
        const bool want = mem && idx && kpool > 0 && ns_len > 0;
        check(call(0, mem ? 1 : 0, idx ? 1 : 0, kpool, ns_len, 0, nullptr) == (want ? 1 : 0), "track", trial);
    }

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t i_cur = (int64_t)(rng() % 5);
        const bool mem = rng() & 1, has_idx = rng() & 1, base_ok = rng() & 1;
        std::vector<int64_t> want;
        if (i_cur == 0 && mem) want.push_back(0);
        if (has_idx) want.push_back(1);
        if (base_ok) want.push_back(2);
        const int64_t got = call(1, i_cur, mem ? 1 : 0, has_idx ? 1 : 0, base_ok ? 1 : 0, 0, nullptr);
        check(got == (int64_t)want.size(), "next event count", trial);
        for (size_t i = 0; i < want.size(); ++i) check(out[i] == want[i], "next event order", trial);
        check(out[3] == i_cur + 1, "next counter", trial);
    }

    for (int trial = 0; trial < 3000; ++trial) {
        const bool base_ok = rng() & 1, idx_ok = rng() & 1, has_idx = rng() & 1, trk = rng() & 1;
        const bool res = base_ok && idx_ok;
        std::vector<int64_t> want;
        if (res && trk) { want = {0, 1, 2}; }
        const int64_t got = call(2, base_ok ? 1 : 0, idx_ok ? 1 : 0, has_idx ? 1 : 0, trk ? 1 : 0, 0, nullptr);
        check(got == (int64_t)want.size(), "apply event count", trial);
        for (size_t i = 0; i < want.size(); ++i) check(out[i] == want[i], "apply event order", trial);
        check(out[3] == (res ? 1 : 0), "apply result", trial);
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const int n = 1 + (int)(rng() % 5);
        std::vector<int64_t> ns(n);
        for (auto & v : ns) v = 1 + (int64_t)(rng() % 4);
        const int64_t i_cur = (int64_t)(rng() % (n + 2)) - 1;
        int64_t want;
        if (i_cur < 0 || i_cur >= n) want = -1;
        else want = ns[i_cur];
        check(call(3, i_cur, 0, 0, 0, n, &ns) == want, "n_stream", trial);
    }

    std::printf("FG313K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
