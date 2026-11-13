// Differential gate for the Goose KV state data plan.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg323dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg323k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg323k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg323k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg323k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg323k_fail(int64_t c) { std::fprintf(stderr, "FG323K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 16), cnt(1);

struct Rec { int64_t kind, a, b, c; };

static std::vector<Rec> run_goose(int64_t v_trans, int64_t n_layer, int64_t n_rot_k, int64_t n_rot_v,
                                  int64_t kv, int64_t gqa, int64_t gqa_max, std::vector<int64_t> & ranges,
                                  int64_t k_type, int64_t k_row, int64_t v_type, int64_t v_row, int64_t v_el) {
    for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
    std::fill(out.begin(), out.end(), -99);
    cnt[0] = 0;
    g_packet[0][0] = v_trans; g_packet[0][1] = n_layer; g_packet[0][2] = n_rot_k; g_packet[0][3] = n_rot_v;
    g_packet[0][4] = kv; g_packet[0][5] = gqa; g_packet[0][6] = gqa_max;
    g_packet[0][7] = (int64_t)ranges.size()/2;
    g_packet[0][8] = (int64_t)(uintptr_t)ranges.data();
    g_packet[0][9] = k_type; g_packet[0][10] = k_row;
    g_packet[0][11] = v_type; g_packet[0][12] = v_row; g_packet[0][13] = v_el;
    g_packet[1][0] = (int64_t)(uintptr_t)out.data();
    g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg323dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    std::vector<Rec> res;
    for (int64_t i = 0; i < cnt[0]; ++i) res.push_back({out[i*4], out[i*4+1], out[i*4+2], out[i*4+3]});
    return res;
}

int main(void) {
    std::mt19937 rng(323011);

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t v_trans = rng() & 1;
        const int64_t n_layer = (int64_t)(rng() % 4);
        const int64_t n_rot_k = (int64_t)(rng() % 5) * 64;
        const int64_t n_rot_v = (int64_t)(rng() % 5) * 64;
        const int64_t kv = 1 + (int64_t)(rng() % 256);
        const int64_t gqa = 1 + (int64_t)(rng() % 32);
        const int64_t gqa_max = gqa + (int64_t)(rng() % 4);
        const int64_t n_ranges = (int64_t)(rng() % 4);
        std::vector<int64_t> ranges;
        int64_t cur = 0;
        for (int64_t r = 0; r < n_ranges; ++r) {
            const int64_t lo = cur + (int64_t)(rng() % 3);
            const int64_t hi = lo + 1 + (int64_t)(rng() % 6);
            ranges.push_back(lo); ranges.push_back(hi);
            cur = hi;
        }
        const int64_t k_type = (int64_t)(rng() % 8), v_type = (int64_t)(rng() % 8);
        const int64_t k_row = (int64_t)(rng() % 256), v_row = (int64_t)(rng() % 256);
        const int64_t v_el = 1 + (int64_t)(rng() % 16);

        std::vector<Rec> want;
        want.push_back({0, 0, v_trans, 0});
        want.push_back({0, 1, n_layer, 0});
        want.push_back({0, 2, n_rot_k, 0});
        want.push_back({0, 3, n_rot_v, 0});
        for (int64_t il = 0; il < n_layer; ++il) {
            want.push_back({0, 4, k_type, 0});
            want.push_back({0, 5, k_row, 0});
            for (int64_t r = 0; r < n_ranges; ++r)
                want.push_back({2, ranges[2*r]*k_row, (ranges[2*r+1]-ranges[2*r])*k_row, 0});
        }
        if (!v_trans) {
            for (int64_t il = 0; il < n_layer; ++il) {
                want.push_back({1, 0, v_type, 0});
                want.push_back({1, 1, v_row, 0});
                for (int64_t r = 0; r < n_ranges; ++r)
                    want.push_back({2, ranges[2*r]*v_row, (ranges[2*r+1]-ranges[2*r])*v_row, 0});
            }
        } else {
            for (int64_t il = 0; il < n_layer; ++il) {
                want.push_back({1, 0, v_type, 0});
                want.push_back({1, 2, v_el, 0});
                want.push_back({1, 3, gqa, 0});
                for (int64_t j = 0; j < gqa; ++j)
                    for (int64_t r = 0; r < n_ranges; ++r)
                        want.push_back({2, (ranges[2*r] + j*kv)*v_el, (ranges[2*r+1]-ranges[2*r])*v_el, 0});
            }
        }

        const auto got = run_goose(v_trans, n_layer, n_rot_k, n_rot_v, kv, gqa, gqa_max, ranges,
                                   k_type, k_row, v_type, v_row, v_el);
        check(got.size() == want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < got.size(); ++i) {
            check(got[i].kind == want[i].kind && got[i].a == want[i].a && got[i].b == want[i].b,
                  "record", trial);
        }
    }

    std::printf("FG323K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
