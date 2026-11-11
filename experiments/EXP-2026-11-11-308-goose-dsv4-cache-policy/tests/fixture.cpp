// Differential gate for the Goose DSV4 cache construction policy.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

extern "C" uint8_t fg308dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg308k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg308k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg308k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg308k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg308k_fail(int64_t c) { std::fprintf(stderr, "FG308K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12);
static uint32_t comp_size(uint32_t kv, uint32_t ratio) { return std::max<uint32_t>(1, (kv + ratio - 1)/ratio); }
static uint32_t pad256(uint32_t x) { return (x + 255u) & ~255u; }

static int64_t call(int op, int64_t n_layer, int64_t head_k, int64_t idx_head,
                    std::vector<int64_t> * ratios, std::vector<int64_t> * ext, bool ext_present, int64_t kv) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = n_layer;
    g_packet[0][3] = head_k;
    g_packet[0][4] = idx_head;
    g_packet[0][5] = ratios ? (int64_t)(uintptr_t)ratios->data() : 0;
    g_packet[0][6] = ext ? (int64_t)(uintptr_t)ext->data() : 0;
    g_packet[0][7] = ext_present ? 1 : 0;
    g_packet[0][8] = kv;
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg308dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(308011);

    for (int trial = 0; trial < 1000; ++trial) {
        const int n_layer = 1 + (int)(rng() % 8);
        const int64_t head_k = 1 + (int64_t)(rng() % 128);
        const int64_t idx_head = 1 + (int64_t)(rng() % 128);
        call(0, n_layer, head_k, idx_head, nullptr, nullptr, false, 0);
        for (int i = 0; i < n_layer; ++i) check(out[i] == 1, "indexer key heads", trial);
        check(out[5*n_layer+0] == idx_head, "k_full", trial);
        check(out[5*n_layer+1] == idx_head, "v_full", trial);
        check(out[5*n_layer+2] == idx_head, "k_swa", trial);
        check(out[5*n_layer+3] == idx_head, "v_swa", trial);
        check(out[5*n_layer+4] == 2, "rope NeoX", trial);

        std::vector<int64_t> ratios(n_layer), ext(n_layer);
        for (int i = 0; i < n_layer; ++i) {
            const int k = (int)(rng() % 3);
            ratios[i] = k == 0 ? 4 : (k == 1 ? 128 : (int64_t)(rng() % 200));
            ext[i] = rng() & 1;
        }
        const bool ext_present = (rng() % 3) == 0;
        call(1, n_layer, head_k, idx_head, &ratios, &ext, ext_present, 0);
        for (int i = 0; i < n_layer; ++i) {
            const bool pass = ext_present ? (ext[i] != 0) : true;
            check(out[3*i+0] == (pass ? 1 : 0), "raw filter", trial);
            check(out[3*i+1] == (pass && ratios[i] == 4 ? 1 : 0), "csa filter", trial);
            check(out[3*i+2] == (pass && ratios[i] == 128 ? 1 : 0), "hca filter", trial);
        }
    }

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t kv = (int64_t)(rng() % 8192);
        call(2, 0, 1, 1, nullptr, nullptr, false, kv);
        check(out[0] == (int64_t)pad256(comp_size((uint32_t)kv, 4)), "csa size", trial);
        check(out[1] == (int64_t)pad256(comp_size((uint32_t)kv, 128)), "hca size", trial);
        check(out[2] == (int64_t)pad256(comp_size((uint32_t)kv, 4)), "lid size", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        const int64_t head_k = 1 + (int64_t)(rng() % 256);
        const int64_t idx_head = 1 + (int64_t)(rng() % 256);
        call(3, 1, head_k, idx_head, nullptr, nullptr, false, 0);
        check(out[0] == 4 && out[1] == 8 && out[2] == 2*head_k, "csa state", trial);
        check(out[3] == 128 && out[4] == 128 && out[5] == head_k, "hca state", trial);
        check(out[6] == 4 && out[7] == 8 && out[8] == 2*idx_head, "lid state", trial);

        const int64_t hk2 = 1 + (int64_t)(rng() % 256);
        call(4, 1, hk2, idx_head, nullptr, nullptr, false, 0);
        check(out[0] == 0 && out[1] == 0, "both unified flags stay false", trial);
        check(out[2] == hk2 && out[3] == hk2, "K-only tweak", trial);
    }

    std::printf("FG308K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
