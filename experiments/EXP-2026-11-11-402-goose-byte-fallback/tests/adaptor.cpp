#include <cstdint>
#include <cstdio>
extern "C" int64_t fg402s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg402s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
static char hexd(int nib) { return nib < 10 ? (char)('0' + nib) : (char)('A' + nib - 10); }
int main(void) {
    check(fg402s_enabled() == 1, "enabled");
    int64_t o2;
    for (int be = 0; be <= 1; ++be) check(fg402s_decide(0, be, 0, 0, 0, 0, &o2) == (be ? 1 : 2), "path");
    for (int b = 0; b < 256; ++b) {
        check(fg402s_decide(1, b, 0, 0, 0, 0, &o2) == hexd((b >> 4) & 0xF), "high nibble");
        check(fg402s_decide(1, b, 1, 0, 0, 0, &o2) == hexd(b & 0xF), "low nibble");
    }
    for (int t = -1; t <= 3; ++t) {
        const bool resolved = t != 0;
        check(fg402s_decide(2, t, 0, 0, 0, 0, &o2) == (resolved ? 1 : 0), "push");
        check(fg402s_decide(3, t, 0, 0, 0, 0, &o2) == (resolved ? 0 : 1), "needs fallback");
    }
    std::printf("FG402S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
