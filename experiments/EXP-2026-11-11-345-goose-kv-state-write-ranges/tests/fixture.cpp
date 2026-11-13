// Differential gate for the Goose KV state write range collection.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg345dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg345k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg345k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg345k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg345k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg345k_fail(int64_t c) { std::fprintf(stderr, "FG345K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 14), cnt(1);
struct Rec { int64_t k, a, b; };

static bool masked_native(int64_t n_swa, int64_t swa_type, int64_t p0, int64_t p1) {
    switch (swa_type) {
        case 0: break;
        case 1: if (p1 - p0 >= n_swa) return true; break;
        case 2: { const int64_t start = (p1 / n_swa) * n_swa; if (p0 < start) return true; } break;
        default: {
            const int64_t half = n_swa / 2;
            const int64_t diff = p1 - p0;
            if (diff < -half || diff > half) return true;
        } break;
    }
    return false;
}

int main(void) {
    std::mt19937 rng(345011);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t n_stream = 1 + (int64_t)(rng() % 3);
        const int64_t size = (int64_t)(rng() % 10);
        const int64_t other = rng() % 10 == 0, partial = rng() & 1, swa_none = rng() & 1;
        const int64_t seq_id = (rng() % 3 == 0) ? -1 : (int64_t)(rng() % 3);
        const int64_t swa_type = (int64_t)(rng() % 4);
        const int64_t n_swa = 1 + (int64_t)(rng() % 8);

        std::vector<int64_t> pos((size_t)(n_stream*size)), mask((size_t)(n_stream*size));
        for (int64_t i = 0; i < n_stream*size; ++i) {
            pos[(size_t)i] = (rng() % 4 == 0) ? -1 : (int64_t)(rng() % 40);
            int64_t m = 0;
            for (int64_t q = 0; q < 3; ++q) if (rng() & 1) m |= (1LL << q);
            mask[(size_t)i] = m;
        }

        std::vector<Rec> want;
        int64_t wret = 0;
        if (!other && !(partial && swa_none)) {
            want.push_back({1, n_stream, 0});
            for (int64_t s = 0; s < n_stream; ++s) {
                const int64_t base = s*size;
                int64_t smax = -1;
                if (seq_id != -1)
                    for (int64_t q = 0; q < size; ++q)
                        if (mask[(size_t)(base+q)] & (1LL << seq_id)) smax = std::max(smax, pos[(size_t)(base+q)]);
                int64_t count = 0, begin = -1;
                std::vector<std::pair<int64_t,int64_t>> runs;
                for (int64_t i = 0; i < size; ++i) {
                    const int64_t p = pos[(size_t)(base+i)];
                    bool add = p != -1 && (seq_id == -1 || (mask[(size_t)(base+i)] & (1LL << seq_id)));
                    if (add && seq_id != -1 && masked_native(n_swa, swa_type, p, smax)) add = false;
                    if (add) {
                        ++count;
                        if (begin < 0) begin = i;
                    } else if (begin >= 0) { runs.push_back({begin, i}); begin = -1; }
                }
                if (begin >= 0) runs.push_back({begin, size});
                for (auto & r : runs) { want.push_back({3, r.first, r.second}); }
                want.push_back({2, s, count});
                if (count == 0) want.push_back({6, s, 0});
                else { want.push_back({4, s, count}); want.push_back({5, s, 0}); }
            }
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][1] = other; g_packet[0][2] = partial; g_packet[0][3] = swa_none;
        g_packet[0][4] = seq_id; g_packet[0][5] = n_stream; g_packet[0][6] = n_swa; g_packet[0][7] = swa_type;
        g_packet[0][8] = size;
        g_packet[0][9] = (int64_t)(uintptr_t)pos.data();
        g_packet[0][10] = (int64_t)(uintptr_t)mask.data();
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
        if (!fg345dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[2][1] == wret, "status", trial);
        check(cnt[0] == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)cnt[0]; ++i)
            check(out[i*3] == want[i].k && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b, "record", trial);
    }

    std::printf("FG345K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
