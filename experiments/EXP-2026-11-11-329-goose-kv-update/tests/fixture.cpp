// Differential gate for the Goose KV update.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg329dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg329k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg329k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg329k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg329k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg329k_fail(int64_t c) { std::fprintf(stderr, "FG329K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);

struct Rec { int64_t kind, a, b; };

static int64_t upd(int64_t other, int64_t n_stream, int64_t n_layers, std::vector<int64_t> & pairs,
                   int64_t do_shift, int64_t can_shift, int64_t rope, int64_t any_rope,
                   int64_t alloc_ok, int64_t compute_ok, int64_t v_present, int64_t & nrec) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99);
    cnt[0] = 0;
    g_packet[0][0] = 0; g_packet[0][1] = other; g_packet[0][2] = n_stream; g_packet[0][3] = n_layers;
    g_packet[0][4] = (int64_t)(uintptr_t)pairs.data(); g_packet[0][5] = (int64_t)pairs.size()/2;
    g_packet[0][6] = do_shift; g_packet[0][7] = can_shift; g_packet[0][8] = rope; g_packet[0][9] = any_rope;
    g_packet[0][10] = alloc_ok; g_packet[0][11] = compute_ok; g_packet[0][12] = v_present;
    g_packet[1][0] = (int64_t)(uintptr_t)out.data();
    g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg329dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    nrec = cnt[0];
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(329011);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_stream = 1 + (int64_t)(rng() % 4);
        const int64_t n_layers = (int64_t)(rng() % 4);
        const int64_t other = rng() % 10 == 0;
        const int64_t do_shift = rng() & 1;
        const int64_t can_shift = (rng() % 5) ? 1 : 0;
        const int64_t rope = rng() & 1;
        const int64_t any_rope = rng() & 1;
        const int64_t alloc_ok = (rng() % 6) ? 1 : 0;
        const int64_t compute_ok = (rng() % 6) ? 1 : 0;
        const int64_t v_present = rng() & 1;

        std::vector<int64_t> pairs;
        const int64_t n_pairs = (n_stream > 1) ? (int64_t)(rng() % 3) : 0;
        for (int64_t p = 0; p < n_pairs; ++p) {
            const int64_t ssrc = (int64_t)(rng() % n_stream);
            int64_t sdst = (int64_t)(rng() % n_stream);
            if (sdst == ssrc) sdst = (ssrc + 1) % n_stream;
            pairs.push_back(ssrc); pairs.push_back(sdst);
        }

        // reference
        std::vector<Rec> want;
        int64_t want_ret = 0;
        if (other) {
            want_ret = 1;
        } else {
            for (int64_t p = 0; p < n_pairs; ++p) {
                for (int64_t il = 0; il < n_layers; ++il) {
                    want.push_back({0, pairs[p*2], pairs[p*2+1]});
                    if (v_present) want.push_back({1, pairs[p*2], pairs[p*2+1]});
                }
            }
            if (do_shift && !can_shift) {
                want_ret = -7;   // native aborts; the port reports -7
            } else if (do_shift) {
                bool ran = rope && any_rope && alloc_ok && compute_ok;
                if (ran) { want.push_back({2, 0, 0}); want_ret = 1; }
                for (int64_t s = 0; s < n_stream; ++s) want.push_back({3, s, 0});
            }
        }

        int64_t nrec = 0;
        const int64_t ret = upd(other, n_stream, n_layers, pairs, do_shift, can_shift, rope, any_rope,
                                alloc_ok, compute_ok, v_present, nrec);
        check(ret == want_ret, "updated flag", trial);
        check(nrec == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)nrec; ++i)
            check(out[i*3] == want[i].kind && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b,
                  "record", trial);

        // can_shift / has_shift
        for (int op = 1; op <= 2; ++op) {
            for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
            const int64_t step35 = rng() & 1;
            const int64_t n_pos = 1 + (int64_t)(rng() % 3);
            std::vector<int64_t> shifts(n_stream);
            for (int64_t s = 0; s < n_stream; ++s) shifts[s] = (rng() % 3) ? 0 : 1;
            g_packet[0][0] = op; g_packet[0][2] = n_stream;
            g_packet[0][13] = op == 1 ? step35 : (int64_t)(uintptr_t)shifts.data();
            g_packet[0][14] = n_pos;
            g_packet[1][0] = (int64_t)(uintptr_t)out.data();
            g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
            g_packet[3][0] = 0; g_packet[3][1] = 0;
            if (!fg329dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
            int64_t want_v;
            if (op == 1) want_v = (!step35 && n_pos <= 1) ? 1 : 0;
            else {
                want_v = 0;
                for (int64_t s = 0; s < n_stream; ++s) if (shifts[s]) want_v = 1;
            }
            check(g_packet[3][1] == want_v, op == 1 ? "can_shift" : "has_shift", trial);
        }
    }

    std::printf("FG329K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
