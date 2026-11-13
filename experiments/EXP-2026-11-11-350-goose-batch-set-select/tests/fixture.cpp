// Differential gate for the batch sequence-set selection.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" uint8_t fg350dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg350k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg350k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg350k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg350k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg350k_fail(int64_t c) { std::fprintf(stderr, "FG350K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 12), cnt(1);
struct Rec { int64_t k, a, b; };

int main(void) {
    std::mt19937 rng(350011);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t n_tokens = 1 + (int64_t)(rng() % 12);
        const int64_t n_sets = 1 + (int64_t)(rng() % 5);
        const int64_t n_ubatch = (int64_t)(rng() % 4);
        const int64_t sequential = rng() & 1;

        std::vector<int64_t> used((size_t)n_tokens), seq_set((size_t)n_tokens), seq_first((size_t)n_tokens);
        for (int64_t i = 0; i < n_tokens; ++i) {
            used[(size_t)i] = (rng() % 4 == 0);
            int64_t m = 0;
            for (int64_t q = 0; q < 4; ++q) if (rng() & 1) m |= (1LL << q);
            seq_set[(size_t)i] = m;
            seq_first[(size_t)i] = (int64_t)(rng() % 5);
        }
        // sets are the distinct sequence-set masks, and their token lists are the rows carrying them --
        // the same shape the native seq_set_map has (a chosen set is always one of these)
        std::vector<int64_t> masks;
        for (int64_t i = 0; i < n_tokens; ++i) {
            bool seen = false;
            for (int64_t m : masks) if (m == seq_set[(size_t)i]) seen = true;
            if (!seen) masks.push_back(seq_set[(size_t)i]);
        }
        const int64_t n_sets_real = (int64_t)masks.size();
        std::vector<int64_t> set_off((size_t)n_sets_real + 1, 0), set_map;
        for (int64_t s = 0; s < n_sets_real; ++s) {
            for (int64_t i = 0; i < n_tokens; ++i) if (seq_set[(size_t)i] == masks[(size_t)s]) set_map.push_back(i);
            set_off[(size_t)s + 1] = (int64_t)set_map.size();
        }

        // reference
        std::vector<Rec> want;
        std::vector<int64_t> chosen_masks;
        int64_t last_seq = -1, n_seqs = 0;
        bool stop = false;
        for (int64_t i = 0; i < n_tokens && !stop; ++i) {
            if (used[(size_t)i]) { want.push_back({3, i, 0}); continue; }
            const int64_t ss = seq_set[(size_t)i];
            bool add = true;
            for (int64_t cs : chosen_masks) if (cs & ss) add = false;
            if (!add) { want.push_back({1, i, 0}); continue; }
            if (sequential && n_seqs != 0 && seq_first[(size_t)i] != last_seq + 1) {
                want.push_back({2, i, 0}); continue;
            }
            want.push_back({0, i, ss});
            chosen_masks.push_back(ss);
            ++n_seqs;
            last_seq = seq_first[(size_t)i];
            if (n_seqs > n_ubatch) stop = true;
        }
        want.push_back({5, n_seqs, 0});
        if (n_seqs > 0) {
            for (int64_t s = 0; s < n_seqs; ++s) {
                int64_t cur = 0;
                const int64_t cntset = set_off[(size_t)s+1] - set_off[(size_t)s];
                while (cur < cntset && used[(size_t)set_map[(size_t)(set_off[(size_t)s] + cur)]]) ++cur;
                want.push_back({4, s, cur});
            }
        }

        std::vector<int64_t> chosen_buf((size_t)n_ubatch + 4, 0);
        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; }
        std::fill(out.begin(), out.end(), -99); cnt[0] = 0;
        g_packet[0][0] = n_tokens;
        g_packet[0][1] = (int64_t)(uintptr_t)used.data();
        g_packet[0][2] = (int64_t)(uintptr_t)seq_set.data();
        g_packet[0][3] = (int64_t)(uintptr_t)seq_first.data();
        g_packet[0][4] = n_ubatch;
        g_packet[0][5] = (int64_t)(uintptr_t)set_map.data();
        g_packet[0][6] = (int64_t)(uintptr_t)set_off.data();
        g_packet[0][7] = n_sets_real;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[1][1] = (int64_t)(uintptr_t)cnt.data();
        g_packet[1][2] = sequential;
        g_packet[1][3] = (int64_t)(uintptr_t)chosen_buf.data();
        g_packet[1][4] = (int64_t)chosen_buf.size();
        if (!fg350dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(g_packet[2][1] == n_seqs, "n_seqs", trial);
        check(cnt[0] == (int64_t)want.size(), "record count", trial);
        for (size_t i = 0; i < want.size() && i < (size_t)cnt[0]; ++i)
            check(out[i*3] == want[i].k && out[i*3+1] == want[i].a && out[i*3+2] == want[i].b, "record", trial);
    }

    std::printf("FG350K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
