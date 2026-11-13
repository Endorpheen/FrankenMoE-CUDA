// Differential gate for the Goose DSV4 policy helpers.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

extern "C" uint8_t fg301dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg301k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg301k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg301k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg301k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg301k_fail(int64_t c) { std::fprintf(stderr, "FG301K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static int64_t run_goose(const std::vector<int64_t> & in) {
    for (int i = 0; i < 10; ++i) g_packet[0][i] = (i < (int)in.size()) ? in[i] : 0;
    g_packet[3][0] = 0;
    g_packet[3][1] = 0;
    if (!fg301dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

static uint32_t comp_size(uint32_t kv, uint32_t ratio) {
    return std::max<uint32_t>(1, (kv + ratio - 1)/ratio);
}
static uint32_t n_used_k_rows(int pos_max, uint32_t ratio, uint32_t kv) {
    if (pos_max < 0) return 0;
    const uint64_t rows = ((uint64_t)pos_max + 1)/ratio;
    return (uint32_t)std::min<uint64_t>(kv, rows);
}

int main(void) {
    std::mt19937 rng(301011);
    static std::vector<int64_t> out(64);

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t kv = (int64_t)(rng() % 4096);
        const int64_t ratio = 1 + (int64_t)(rng() % 200);
        std::vector<int64_t> in(10, 0);
        in[0] = 0; in[1] = (int64_t)(uintptr_t)out.data(); in[2] = kv; in[3] = ratio;
        check(run_goose(in) == (int64_t)comp_size((uint32_t)kv, (uint32_t)ratio), "comp_size", trial);
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t pm = (int64_t)(rng() % 4096) - 10;
        const int64_t ratio = 1 + (int64_t)(rng() % 200);
        const int64_t kv = (int64_t)(rng() % 4096);
        std::vector<int64_t> in(10, 0);
        in[0] = 1; in[1] = (int64_t)(uintptr_t)out.data(); in[2] = pm; in[3] = ratio; in[4] = kv;
        check(run_goose(in) == (int64_t)n_used_k_rows((int)pm, (uint32_t)ratio, (uint32_t)kv),
              "n_used_k_rows", trial);
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const int64_t ns = (int64_t)(rng() % 5);
        const int64_t sid = (int64_t)(rng() % 8) - 2;
        const int64_t size = (int64_t)(rng() % 10000);
        std::vector<int64_t> in(10, 0);
        in[0] = 2; in[1] = (int64_t)(uintptr_t)out.data(); in[2] = ns; in[3] = sid; in[4] = size;
        int64_t want;
        if (ns <= 1) want = 0;
        else if (sid < 0 || sid >= ns) want = -1;   // the native throws; the FFI reports -1
        else want = sid*size;
        check(run_goose(in) == want, "stream_offset", trial);
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const int n = 1 + (int)(rng() % 8);
        std::vector<int64_t> nsid(n);
        int64_t want = 0;
        for (int i = 0; i < n; ++i) { nsid[i] = (int64_t)(rng() % 4); if (nsid[i] > 1) want = 1; }
        std::vector<int64_t> in(10, 0);
        in[0] = 3; in[1] = (int64_t)(uintptr_t)out.data(); in[7] = n;
        in[6] = (int64_t)(uintptr_t)nsid.data();
        check(run_goose(in) == want, "has_coupled", trial);
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const int width = 1 + (int)(rng() % 4);
        const int n = 1 + (int)(rng() % 6);
        std::vector<int64_t> sid(width * n, 0), nsid(n);
        for (int i = 0; i < n; ++i) {
            nsid[i] = 1 + (int)(rng() % width);
            for (int j = 0; j < width; ++j) sid[i*width + j] = (int64_t)(rng() % 6);
        }
        const int i = (int)(rng() % n);
        const int64_t want_id = (int64_t)(rng() % 7);
        int64_t want = 0;
        for (int j = 0; j < nsid[i]; ++j) if (sid[i*width + j] == want_id) want = 1;
        std::vector<int64_t> in(10, 0);
        in[0] = 4; in[1] = (int64_t)(uintptr_t)out.data();
        in[2] = i; in[3] = want_id; in[8] = width;
        in[6] = (int64_t)(uintptr_t)sid.data();
        in[7] = n;
        in[9] = (int64_t)(uintptr_t)nsid.data();
        check(run_goose(in) == want, "token_has_seq", trial);
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const int64_t ns = (int64_t)(rng() % 6);
        const int64_t nu = (int64_t)(rng() % 6);
        const int64_t coupled = rng() & 1;
        const int64_t want = (ns <= 1 || nu <= 1 || coupled) ? 1 : nu;
        std::vector<int64_t> in(10, 0);
        in[0] = 5; in[1] = (int64_t)(uintptr_t)out.data(); in[2] = ns; in[3] = nu; in[4] = coupled;
        check(run_goose(in) == want, "comp_graph_n_stream", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        const int64_t h = 1 + (int64_t)(rng() % 512);
        std::vector<int64_t> in(10, 0);
        in[0] = 6; in[1] = (int64_t)(uintptr_t)out.data(); in[2] = h;
        run_goose(in);
        check(out[0] == h && out[1] == h, "make_k_only", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        const int64_t ns = 1 + (int64_t)(rng() % 8);
        const int64_t rs = (int64_t)(rng() % 8);
        std::vector<int64_t> in(10, 0);
        in[0] = 7; in[1] = (int64_t)(uintptr_t)out.data(); in[2] = ns; in[3] = rs;
        check(run_goose(in) == ns*(1 + rs), "state planes", trial);
    }

    std::printf("FG301K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
