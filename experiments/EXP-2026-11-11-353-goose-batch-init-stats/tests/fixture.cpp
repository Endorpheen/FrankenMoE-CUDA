// Differential gate for the init output/coupling/set section.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg353dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg353k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg353k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg353k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg353k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg353k_fail(int64_t c) { std::fprintf(stderr, "FG353K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 13), cnt(1);
struct Rec { int64_t k, a, b; };

int main(void) {
    std::mt19937 rng(353011);

    for (int trial = 0; trial < 6000; ++trial) {
        const int64_t n = 1 + (int64_t)(rng() % 8);
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 5);
        const int64_t output_all = rng() & 1;
        std::vector<int64_t> flags((size_t)n), order((size_t)n), nseqid((size_t)n), seqid((size_t)n * 4, 0);
        for (int64_t i = 0; i < n; ++i) {
            flags[(size_t)i] = rng() & 1;
            order[(size_t)i] = (rng() % 4 == 0) ? 1 + (int64_t)(rng() % 3) : 0;
            nseqid[(size_t)i] = 1 + (int64_t)(rng() % 3);
            for (int64_t j = 0; j < nseqid[(size_t)i]; ++j) seqid[(size_t)(i*4+j)] = (int64_t)(rng() % n_seq_max);
        }

        // reference
        std::vector<Rec> want;
        bool any_missing = false;
        for (int64_t i = 0; i < n; ++i) if (!flags[(size_t)i]) any_missing = true;
        const bool override_all = output_all && any_missing;
        if (override_all) want.push_back({1, 0, 0});
        int64_t n_outputs = 0;
        for (int64_t i = 0; i < n; ++i) {
            const int64_t v = override_all ? 1 : flags[(size_t)i];
            want.push_back({0, i, v});
            if (v) ++n_outputs;
        }
        want.push_back({3, n_outputs, 0});
        bool has_order = false;
        for (int64_t i = 0; i < n; ++i) if (order[(size_t)i]) has_order = true;
        if (has_order) for (int64_t i = 0; i < n; ++i) want.push_back({2, i, order[(size_t)i]});
        bool has_cpl = false;
        for (int64_t i = 0; i < n; ++i) {
            const int64_t s0 = seqid[(size_t)(i*4)];
            for (int64_t j = 1; j < nseqid[(size_t)i]; ++j) {
                want.push_back({4, seqid[(size_t)(i*4+j)], s0});
                has_cpl = true;
            }
        }
        want.push_back({5, has_cpl ? 1 : 0, 0});
        // sets and their first-appearance map
        std::vector<int64_t> masks, first_entry;
        for (int64_t i = 0; i < n; ++i) {
            int64_t m = 0;
            for (int64_t j = 0; j < nseqid[(size_t)i]; ++j) m |= (1LL << seqid[(size_t)(i*4+j)]);
            want.push_back({6, i, m});
            bool first = true;
            for (int64_t q = 0; q < (int64_t)masks.size(); ++q) if (masks[(size_t)q] == m) first = false;
            if (first) { masks.push_back(m); first_entry.push_back(i); want.push_back({7, (int64_t)masks.size()-1, i}); }
        }
        int64_t u = 0;
        for (int64_t s = 0; s < n_seq_max; ++s) {
            bool present = false;
            for (int64_t r = 0; r < n && !present; ++r)
                for (int64_t k = 0; k < nseqid[(size_t)r]; ++k) if (seqid[(size_t)(r*4+k)] == s) present = true;
            if (present) { want.push_back({8, s, u}); want.push_back({9, s, u}); ++u; }
        }

        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][0] = n;
        g_packet[0][1] = (int64_t)(uintptr_t)flags.data();
        g_packet[0][2] = output_all;
        g_packet[0][3] = (int64_t)(uintptr_t)order.data();
        g_packet[0][4] = n_seq_max;
        g_packet[0][5] = (int64_t)(uintptr_t)nseqid.data();
        g_packet[0][6] = (int64_t)(uintptr_t)seqid.data();
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
        if (!fg353dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[2][1] == n_outputs, "n_outputs", trial);
        check(cnt[0] == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)cnt[0]; ++i)
            check(out[i*3] == want[i].k && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b, "record", trial);
    }

    std::printf("FG353K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
