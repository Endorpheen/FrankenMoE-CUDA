// Differential gate for the Goose base hybrid memory policy.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg315dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg315k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg315k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg315k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg315k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg315k_fail(int64_t c) { std::fprintf(stderr, "FG315K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 10);

static int64_t call(int op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t n,
                    std::vector<int64_t> * arr, std::vector<int64_t> * arr2, std::vector<int64_t> * arr3) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    std::fill(out.begin(), out.end(), -99);
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = a; g_packet[0][3] = b; g_packet[0][4] = c; g_packet[0][5] = d;
    g_packet[0][6] = n;
    g_packet[0][7] = arr ? (int64_t)(uintptr_t)arr->data() : 0;
    g_packet[0][8] = arr2 ? (int64_t)(uintptr_t)arr2->data() : 0;
    g_packet[0][9] = arr3 ? (int64_t)(uintptr_t)arr3->data() : 0;
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg315dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(315011);

    for (int trial = 0; trial < 2000; ++trial) {
        const int n_layer = 1 + (int)(rng() % 8);
        std::vector<int64_t> is_recr(n_layer), fa(n_layer), fr(n_layer);
        for (int i = 0; i < n_layer; ++i) { is_recr[i] = rng() & 1; fa[i] = rng() & 1; fr[i] = rng() & 1; }
        const bool attn_present = (rng() % 3) == 0;
        const bool recr_present = (rng() % 3) == 0;

        call(0, 0, 0, attn_present ? 1 : 0, recr_present ? 1 : 0, n_layer,
             &is_recr, &fa, &fr);
        for (int i = 0; i < n_layer; ++i) {
            const int64_t want_a = attn_present ? fa[i] : (is_recr[i] == 0 ? 1 : 0);
            const int64_t want_r = recr_present ? fr[i] : (is_recr[i] != 0 ? 1 : 0);
            check(out[2*i] == want_a, "attention filter", trial);
            check(out[2*i+1] == want_r, "recurrent filter", trial);
        }
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const bool embd_all = rng() & 1;
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        const int64_t n_rs = (int64_t)(rng() % 3);
        const int64_t got = call(1, embd_all ? 1 : 0, n_stream, n_rs, 0, 0, nullptr, nullptr, nullptr);
        if (embd_all) {
            check(got == 0, "split by sequence", trial);
        } else {
            check(got == 1, "equal split", trial);
            check(out[0] == (n_stream == 1 ? 1 : 2), "sequential flag", trial);
            check(out[1] == (n_rs > 0 ? n_rs + 1 : 0), "rollback window", trial);
        }
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const bool recr_ok = rng() & 1, heads_empty = rng() & 1;
        std::vector<int64_t> want;
        want.push_back(0);
        if (recr_ok) {
            want.push_back(1);
        }
        const bool ok = recr_ok && !heads_empty;
        // the events: recurrent always, attention only if the recurrent part went through
        const int64_t got = call(2, recr_ok ? 1 : 0, heads_empty ? 1 : 0, 0, 0, 0, nullptr, nullptr, nullptr);
        check(got == (ok ? 1 : 0), "prepare result", trial);
        check(out[0] == 0, "recurrent is prepared first", trial);
        if (recr_ok) check(out[1] == 1, "attention prepared second", trial);
    }

    for (int trial = 0; trial < 1000; ++trial) {
        const bool unified = rng() & 1;
        const int64_t n_seq = 1 + (int64_t)(rng() % 6);
        check(call(3, unified ? 1 : 0, 0, 0, 0, n_seq, nullptr, nullptr, nullptr) == (unified ? 1 : n_seq),
              "attention streams", trial);
    }

    std::printf("FG315K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
