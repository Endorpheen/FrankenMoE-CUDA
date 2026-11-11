#include <cstdint>
#include <cstdio>
extern "C" int64_t fg398s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg398s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg398s_enabled() == 1, "enabled");
    int64_t o2;
    for (int64_t row = 0; row <= 3; ++row)
        for (int64_t per = 0; per <= 4; ++per)
            for (int64_t i = 0; i <= 4; ++i) {
                check(fg398s_decide(0, row, per, i, 0, 0, &o2) == row * per + i, "dst");
                check(fg398s_decide(1, row, per, i, 0, 0, &o2) == row * per + i, "src");
            }
    for (int64_t base = 0; base <= 9; ++base)
        for (int64_t add = 0; add <= 4; ++add)
            check(fg398s_decide(2, base, add, 0, 0, 0, &o2) == base + add, "pool");
    std::printf("FG398S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
