#include <cstdint>
#include <cstdio>
extern "C" int64_t fg390s_decide(int64_t op, int64_t a, int64_t b);
extern "C" int fg390s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg390s_enabled() == 1, "enabled");
    for (int ps = 0; ps <= 1; ++ps)
        for (int attr = 0; attr <= 1; ++attr)
            check(fg390s_decide(0, ps, attr) == ((!ps && attr) ? 1 : 0), "skip");
    for (int bit = 0; bit <= 1; ++bit)
        for (int rem = -2; rem <= 4; ++rem)
            check(fg390s_decide(1, bit, rem) == ((bit && rem > 0) ? 1 : 0), "lstrip"),
            check(fg390s_decide(2, bit, rem) == ((bit && rem > 0) ? 1 : 0), "rstrip");
    std::printf("FG390S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
