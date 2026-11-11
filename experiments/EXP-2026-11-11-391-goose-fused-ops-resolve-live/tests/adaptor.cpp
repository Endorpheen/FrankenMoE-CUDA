#include <cstdint>
#include <cstdio>
extern "C" int64_t fg391s_decide(int64_t op, int64_t a, int64_t b);
extern "C" int fg391s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg391s_enabled() == 1, "enabled");
    for (int en = 0; en <= 1; ++en)
        for (int au = 0; au <= 1; ++au)
            check(fg391s_decide(0, en, au) == (en && au ? 1 : 0), "probe");
    for (int per = 0; per <= 5; ++per)
        for (int ns = 0; ns <= 5; ++ns)
            check(fg391s_decide(1, per, ns) == per * ns, "width");
    for (int mm = 0; mm <= 1; ++mm) check(fg391s_decide(2, mm, 0) == (mm ? 0 : 1), "verdict");
    std::printf("FG391S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
