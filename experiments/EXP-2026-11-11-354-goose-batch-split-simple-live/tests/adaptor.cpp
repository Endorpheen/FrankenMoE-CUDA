// Gate: the bridge must return exactly what the native walk returns, and must leave `used` identical.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

extern "C" int64_t fg351s_split_simple(int64_t n_tokens, int64_t n_ubatch, const int32_t * used,
                                       int32_t * out_idxs, int64_t out_cap, int32_t * used_out);
extern "C" int fg351s_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(354011);
    check(fg351s_enabled() == 1, "enabled by default", 0);

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t n = 1 + (int64_t)(rng() % 16);
        const int64_t n_ubatch = 1 + (int64_t)(rng() % 6);
        std::vector<int32_t> used((size_t)n);
        for (auto & x : used) x = (rng() % 3 == 0) ? 1 : 0;

        // native
        std::vector<int32_t> nused = used, nidxs;
        int64_t cur = 0;
        while (cur < n && nused[(size_t)cur]) ++cur;
        if (cur < n) {
            while (true) {
                nidxs.push_back((int32_t)cur);
                nused[(size_t)cur] = 1;
                ++cur;
                if (cur >= n) break;
                if ((int64_t)nidxs.size() >= n_ubatch) break;
            }
        }

        std::vector<int32_t> gidxs((size_t)(n_ubatch + 2), -1), gused((size_t)n, -1);
        const int64_t got = fg351s_split_simple(n, n_ubatch, used.data(), gidxs.data(),
                                                (int64_t)gidxs.size(), gused.data());
        check(got == (int64_t)nidxs.size(), "count", trial);
        for (size_t i = 0; i < nidxs.size() && i < (size_t)got; ++i) check(gidxs[i] == nidxs[i], "index", trial);
        for (int64_t i = 0; i < n; ++i) check(gused[(size_t)i] == nused[(size_t)i], "used", trial);
    }

    std::printf("FG351S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
