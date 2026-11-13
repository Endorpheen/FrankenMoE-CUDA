#include <cstdint>
#include <cstdio>
extern "C" int64_t fg376s_decide(int64_t op, int64_t a, int64_t b);
extern "C" int fg376s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg376s_enabled() == 1, "enabled");
    for (int a = 0; a <= 1; ++a)
        for (int b = 0; b <= 1; ++b)
            check(fg376s_decide(0, a, b) == (a && b ? 1 : 0), "apply");
    for (int i = 0; i <= 5; ++i)
        for (int n = 1; n <= 5; ++n) {
            const int at = i + 1;
            check(fg376s_decide(1, i, n) == (at >= n ? 1 : 0), "next");
        }
    std::printf("FG376S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
