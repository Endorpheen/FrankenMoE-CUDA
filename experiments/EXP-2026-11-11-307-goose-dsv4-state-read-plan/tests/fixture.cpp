// Differential gate for the Goose DSV4 compressed-state restore plan.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg307dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg307k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg307k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg307k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg307k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg307k_fail(int64_t c) { std::fprintf(stderr, "FG307K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(307011);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t version_ref = (rng() % 8 == 0) ? (int64_t)(rng() % 4) : 1;
        const int64_t ratio = 1 + (int64_t)(rng() % 8);
        const int64_t state_size = 1 + (int64_t)(rng() % 16);
        const int64_t n_embd = 1 + (int64_t)(rng() % 32);
        const int64_t n_layers = (int64_t)(rng() % 5);
        int64_t ratio_ref = ratio, state_size_ref = state_size, n_embd_ref = n_embd, n_layer_ref = n_layers;
        switch (rng() % 5) {
            case 0: ratio_ref = ratio + 1; break;
            case 1: state_size_ref = state_size + 1; break;
            case 2: n_embd_ref = n_embd + 1; break;
            case 3: n_layer_ref = n_layers > 0 ? n_layers - 1 : n_layers + 1; break;
            default: break;
        }
        const int64_t n_stream = 1 + (int64_t)(rng() % 5);
        const int64_t seq_id = (int64_t)(rng() % (n_stream + 2)) - 1;
        int64_t ns = (int64_t)(rng() % (n_stream + 2));

        std::vector<int64_t> il(n_layers);
        for (int64_t i = 0; i < n_layers; ++i) il[i] = i;
        if (n_layers > 0 && rng() % 8 == 0) il[rng() % n_layers] = (int64_t)(rng() % 8) + 100;

        // reference
        int64_t want = 0;
        if (version_ref != 1) want = -1;
        else if (ratio_ref != ratio || state_size_ref != state_size || n_embd_ref != n_embd) want = -2;
        else if (n_layer_ref != n_layers) want = -3;
        else {
            bool ok_range = true;
            if (seq_id >= 0) {
                if (ns != 1) ok_range = false;
                else if (n_stream > 1 && seq_id >= n_stream) ok_range = false;
            } else if (ns != n_stream) ok_range = false;
            if (!ok_range) want = -4;
            else {
                int64_t reads = 0; bool bad = false;
                for (int64_t i = 0; i < n_layers && !bad; ++i) {
                    if (il[i] != i) bad = true; else reads += 2;
                }
                want = bad ? -5 : reads;
            }
        }

        for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
        g_packet[0][0] = version_ref;
        g_packet[0][1] = ratio_ref;
        g_packet[0][2] = state_size_ref;
        g_packet[0][3] = n_embd_ref;
        g_packet[0][4] = ns;
        g_packet[0][5] = n_layer_ref;
        g_packet[0][6] = seq_id;
        g_packet[0][7] = n_stream;
        g_packet[0][8] = ratio;
        g_packet[0][9] = state_size;
        g_packet[0][10] = n_embd;
        g_packet[0][11] = n_layers;
        g_packet[0][12] = (int64_t)(uintptr_t)il.data();
        g_packet[3][0] = 0;
        g_packet[3][1] = 0;
        if (!fg307dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
        const int64_t got = g_packet[3][1];
        check(got == want, "restore plan", trial);
    }

    std::printf("FG307K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
