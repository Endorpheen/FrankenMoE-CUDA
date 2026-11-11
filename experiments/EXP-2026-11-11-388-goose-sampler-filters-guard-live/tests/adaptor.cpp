#include <cstdint>
#include <cstdio>
extern "C" int64_t fg388s_decide(int64_t op, int64_t a, int64_t b);
extern "C" int fg388s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg388s_enabled() == 1, "enabled");
    const int64_t vals[5] = {-1000000, 0, 500000, 1000000, 2000000};
    for (int64_t v : vals)
        for (int64_t win = 0; win <= 2; ++win) {
            check(fg388s_decide(0, v, win) == ((v <= 0 || win == 0) ? 1 : 0), "dry mul/window");
            check(fg388s_decide(1, v, 0) == (v < 1000000 ? 1 : 0), "dry base");
            check(fg388s_decide(2, v, win) == ((v <= 0 || win <= 1) ? 1 : 0), "sigma");
            check(fg388s_decide(3, v, win) == ((v <= 0 || win < 2) ? 1 : 0), "xtc prob");
            check(fg388s_decide(4, v, 0) == (v > 500000 ? 1 : 0), "xtc threshold");
        }
    std::printf("FG388S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
