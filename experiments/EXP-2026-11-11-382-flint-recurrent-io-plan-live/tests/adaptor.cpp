#include <cstdint>
#include <cstdio>
extern "C" int64_t fg382s_decide(int64_t op, int64_t a);
extern "C" int fg382s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg382s_enabled() == 1, "enabled");
    for (int a = -5; a <= 5; ++a) check(fg382s_decide(0, a) == (a == 0 ? 1 : 0), "skip layer");
    for (int a = -5; a <= 5; ++a) check(fg382s_decide(1, a) == (a != 0 ? 1 : 0), "has s");
    for (int a = -5; a <= 5; ++a) check(fg382s_decide(0, a) != fg382s_decide(1, a), "the two plans disagree on every layer");
    std::printf("FG382S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
