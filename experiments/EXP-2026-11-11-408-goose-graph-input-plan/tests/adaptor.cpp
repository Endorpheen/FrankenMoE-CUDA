#include <cstdint>
#include <cstdio>
extern "C" int64_t fg408s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg408s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg408s_enabled() == 1, "enabled");
    int64_t o2;
    for (int arch = 0; arch <= 1; ++arch)
        for (int ctx = 0; ctx <= 1; ++ctx)
            check(fg408s_decide(0, arch, ctx, 0, 0, 0, &o2) == ((arch && ctx) ? 1 : 0), "mixed");
    for (int m = 0; m <= 1; ++m)
        for (int64_t r = 0; r <= 8; r += 4)
            check(fg408s_decide(1, m, r, 0, 0, 0, &o2) == (m ? r : 0), "rows");
    for (int64_t n = 0; n <= 8; n += 4)
        for (int64_t inp = 0; inp <= 12; inp += 4)
            check(fg408s_decide(2, n, inp, 0, 0, 0, &o2) == (inp > n ? inp - n : 0), "pad");
    std::printf("FG408S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
