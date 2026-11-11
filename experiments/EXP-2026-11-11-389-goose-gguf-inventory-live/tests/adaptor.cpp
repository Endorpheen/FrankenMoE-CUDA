#include <cstdint>
#include <cstdio>
#include <random>
#include <map>
extern "C" int64_t fg389s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t * out2);
extern "C" int fg389s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg389s_enabled() == 1, "enabled");
    int64_t o2;
    for (int f = 0; f <= 1; ++f) check(fg389s_decide(0, f, 0, 0, &o2) == (f ? 1 : 0), "duplicate");
    for (int t = 0; t <= 5; ++t)
        for (int cnt = 0; cnt <= 6; ++cnt)
            for (int mx = 0; mx <= 6; ++mx) {
                const int64_t r = fg389s_decide(1, t, cnt, mx, &o2);
                if (cnt > mx) { check(r == t, "dominant type kept"); check(o2 == cnt, "dominant max updated"); }
                else          { check(r == -1, "dominant rejected"); check(o2 == mx, "dominant max kept"); }
            }
    for (int c = -2; c <= 10; ++c) check(fg389s_decide(2, c, 0, 0, &o2) == c + 1, "step");
    // a whole inventory, as the loader walks it: mixed types, ties included
    std::mt19937 rng(99);
    for (int trial = 0; trial < 200; ++trial) {
        std::map<int, int> counts; int best = -1, best_n = 0, ref_type = -1, ref_n = 0;
        const int items = 1 + (int) (rng() % 300);
        for (int i = 0; i < items; ++i) {
            const int t = (int) (rng() % 7);
            ++counts[t];
            if (counts[t] > best_n) { best = t; best_n = counts[t]; }
            // reference: the loader's own loop
            if (counts[t] > ref_n) { ref_type = t; ref_n = counts[t]; }
            const int64_t got = fg389s_decide(2, counts[t] - 1, 0, 0, &o2);
            check(got == counts[t], "step matches map");
        }
        check(best == ref_type && best_n == ref_n, "dominant type agrees");
    }
    std::printf("FG389S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
