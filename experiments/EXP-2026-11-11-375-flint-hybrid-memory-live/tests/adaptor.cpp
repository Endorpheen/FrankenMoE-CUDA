#include <cstdint>
#include <cstdio>
extern "C" int64_t fg375s_decide(int64_t op, int64_t a, int64_t b);
extern "C" int fg375s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg375s_enabled() == 1, "enabled");
    for (int a = -1; a <= 3; ++a)
        for (int b = -1; b <= 3; ++b) {
            check(fg375s_decide(0, a, b) == (a > b ? a : b), "pos_min");
            check(fg375s_decide(1, a, b) == (a < b ? a : b), "pos_max");
            check(fg375s_decide(2, a, b) == (a != 0 && b == 0 ? 1 : 0), "clear");
        }
    std::printf("FG375S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
