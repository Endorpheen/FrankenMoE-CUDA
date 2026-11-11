// Gate: the bridge must reproduce the native selection (accepted rows) and cursors.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

extern "C" int64_t fg350s_select(int64_t n_tokens, const int32_t * used, const uint64_t * seq_set,
                                 const int32_t * seq_first, int64_t n_ubatch, const int32_t * set_map,
                                 const int32_t * set_off, int64_t n_sets, int64_t sequential,
                                 int32_t * out_rows, int32_t * out_rows_n, int32_t * out_cur);
extern "C" int fg350s_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(360011);
    check(fg350s_enabled() == 1, "enabled by default", 0);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n_tokens = 1 + (int64_t)(rng() % 12);
        const int64_t n_ubatch = (int64_t)(rng() % 4);
        const int64_t sequential = rng() & 1;
        std::vector<int32_t> used((size_t)n_tokens), first((size_t)n_tokens);
        std::vector<uint64_t> ss((size_t)n_tokens);
        for (int64_t i = 0; i < n_tokens; ++i) {
            used[(size_t)i] = (rng() % 4 == 0);
            uint64_t m = 0;
            for (int64_t q = 0; q < 4; ++q) if (rng() & 1) m |= (1ULL << q);
            ss[(size_t)i] = m;
            first[(size_t)i] = (int32_t)(rng() % 5);
        }
        std::vector<uint64_t> masks;
        for (int64_t i = 0; i < n_tokens; ++i) {
            bool seen = false;
            for (uint64_t m : masks) if (m == ss[(size_t)i]) seen = true;
            if (!seen) masks.push_back(ss[(size_t)i]);
        }
        const int64_t n_sets = (int64_t)masks.size();
        std::vector<int32_t> off((size_t)n_sets + 1, 0), map;
        for (int64_t s = 0; s < n_sets; ++s) {
            for (int64_t i = 0; i < n_tokens; ++i) if (ss[(size_t)i] == masks[(size_t)s]) map.push_back((int32_t)i);
            off[(size_t)s + 1] = (int32_t)map.size();
        }

        // native
        std::vector<int32_t> w_rows;
        std::vector<uint64_t> chosen;
        int64_t last = -1, n_seqs = 0;
        bool stop = false;
        for (int64_t i = 0; i < n_tokens && !stop; ++i) {
            if (used[(size_t)i]) continue;
            bool add = true;
            for (uint64_t cs : chosen) if ((cs & ss[(size_t)i]) != 0) add = false;
            if (!add) continue;
            if (sequential && n_seqs != 0 && first[(size_t)i] != last + 1) continue;
            chosen.push_back(ss[(size_t)i]);
            w_rows.push_back((int32_t)i);
            ++n_seqs;
            last = first[(size_t)i];
            if (n_seqs > n_ubatch) stop = true;
        }
        std::vector<int32_t> w_cur;
        if (n_seqs > 0) {
            for (int64_t s = 0; s < n_seqs; ++s) {
                int32_t cur = 0;
                const int32_t cnt = off[(size_t)s+1] - off[(size_t)s];
                while (cur < cnt && used[(size_t)map[(size_t)(off[(size_t)s] + cur)]]) ++cur;
                w_cur.push_back(cur);
            }
        }

        std::vector<int32_t> p_rows((size_t)n_tokens + 4, -1), p_cur((size_t)n_ubatch + 4, -1);
        int32_t p_rows_n = -1;
        const int64_t got = fg350s_select(n_tokens, used.data(), ss.data(), first.data(), n_ubatch,
                                          map.data(), off.data(), n_sets, sequential,
                                          p_rows.data(), &p_rows_n, p_cur.data());
        check(got == n_seqs, "n_seqs", trial);
        check(p_rows_n == (int32_t)w_rows.size(), "row count", trial);
        for (size_t i = 0; i < w_rows.size() && i < (size_t)p_rows_n; ++i) check(p_rows[i] == w_rows[i], "row", trial);
        for (size_t i = 0; i < w_cur.size() && i < (size_t)p_rows_n; ++i) check(p_cur[i] == w_cur[i], "cursor", trial);
    }

    std::printf("FG350S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
