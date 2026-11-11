// Differential gate for the Goose DSA KV cache policy.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg300dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg300k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg300k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg300k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg300k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg300k_fail(int64_t c) { std::fprintf(stderr, "FG300K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static int64_t run_goose(int op, const std::vector<int64_t> & in) {
    for (int i = 0; i < 13; ++i) g_packet[0][i] = (i < (int)in.size()) ? in[i] : 0;
    g_packet[3][0] = 0;
    g_packet[3][1] = 0;
    if (!fg300dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(300011);
    static std::vector<int64_t> out(1 << 12);

    for (int trial = 0; trial < 1000; ++trial) {
        const int n_layer = 1 + (int)(rng() % 12);
        const int64_t idx_head = 1 + (int64_t)(rng() % 256);
        const bool unified = (rng() & 1) != 0;
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 16);
        std::vector<int64_t> arr(n_layer);
        for (int i = 0; i < n_layer; ++i) arr[i] = 1 + (int64_t)(rng() % 8);

        std::vector<int64_t> in(13, 0);
        in[0] = 0; in[1] = (int64_t)(uintptr_t)out.data(); in[2] = n_layer;
        in[3] = (int64_t)(uintptr_t)arr.data(); in[4] = idx_head;
        in[5] = unified ? 1 : 0; in[6] = n_seq_max;
        run_goose(0, in);

        for (int i = 0; i < n_layer; ++i) check(out[i] == 1, "one key head", trial);
        check(out[n_layer] == idx_head, "indexer head size", trial);
        check(out[n_layer + 1] == 2, "rope is NeoX", trial);
        check(out[n_layer + 2] == (unified ? 1 : n_seq_max), "n_stream", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        const int n_layer = 1 + (int)(rng() % 10);
        std::vector<int64_t> f(n_layer), fi(n_layer);
        for (int i = 0; i < n_layer; ++i) { f[i] = rng() & 1; fi[i] = rng() & 1; }
        std::vector<int64_t> in(13, 0);
        in[0] = 1; in[1] = (int64_t)(uintptr_t)out.data(); in[2] = n_layer;
        in[7] = (int64_t)(uintptr_t)f.data(); in[8] = (int64_t)(uintptr_t)fi.data();
        run_goose(1, in);
        for (int i = 0; i < n_layer; ++i) {
            check(out[2*i] == f[i], "mla filter", trial);
            check(out[2*i+1] == fi[i], "indexer filter", trial);
        }
    }

    for (int trial = 0; trial < 200; ++trial) {
        const int64_t a = rng() & 1, b = rng() & 1;
        std::vector<int64_t> in(13, 0);
        in[0] = 2; in[9] = a; in[10] = b;
        check(run_goose(2, in) == ((a && b) ? 1 : 0), "seq_rm and", trial);
    }

    for (int trial = 0; trial < 200; ++trial) {
        const int64_t a = (int64_t)(rng() % 1000), b = (int64_t)(rng() % 1000);
        std::vector<int64_t> in(13, 0);
        in[0] = 3; in[9] = a; in[10] = b;
        check(run_goose(3, in) == a, "position source is the MLA cache", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        const int64_t ms = rng() & 1, ls = rng() & 1, mz = (int64_t)(rng() % 4), lz = (int64_t)(rng() % 4);
        std::vector<int64_t> in(13, 0);
        in[0] = 4; in[9] = ms; in[10] = ls; in[11] = mz; in[12] = lz;
        const int64_t want = (ms && ls && mz == lz) ? 1 : 0;
        check(run_goose(4, in) == want, "can_shift", trial);
    }

    for (int trial = 0; trial < 200; ++trial) {
        const int64_t fail = rng() & 1;
        std::vector<int64_t> in(13, 0);
        in[0] = 5; in[1] = (int64_t)(uintptr_t)out.data(); in[9] = fail;
        run_goose(5, in);
        check(out[0] == 1, "mla is read first", trial);
        check(out[1] == (fail ? 0 : 1), "indexer read result", trial);
        check(out[2] == (fail ? 1 : 0), "mla rolled back on failure", trial);
    }

    std::printf("FG300K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
