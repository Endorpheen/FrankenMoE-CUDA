#include <cstdint>
#include <cstdio>
extern "C" int64_t fg401s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg401s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg401s_enabled() == 1, "enabled");
    int64_t o2;
    for (int sp = 0; sp <= 1; ++sp)
        for (int at = 0; at <= 1; ++at)
            check(fg401s_decide(0, sp, at, 0, 0, 0, &o2) == ((!sp && at) ? 1 : 0), "skip");
    for (int ls = -2; ls <= 3; ++ls)
        for (int sz = -1; sz <= 3; ++sz)
            check(fg401s_decide(1, ls, sz, 0, 0, 0, &o2) == ((ls > 0 && sz > 0) ? 1 : 0), "lstrip");
    for (int n = -1; n <= 5; ++n) check(fg401s_decide(2, n, 0, 0, 0, 0, &o2) == n, "bound");
    std::printf("FG401S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
