#include <cstdint>
#include <cstdio>
extern "C" int64_t fg381s_decide(int64_t op, int64_t a);
extern "C" int fg381s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg381s_enabled() == 1, "enabled");
    for (int s = -3; s <= 3; ++s) check(fg381s_decide(0, s) == (s == -1 ? 1 : 0), "clear once");
    for (int c = -2; c <= 4; ++c) check(fg381s_decide(1, c) == (c > 0 ? 1 : 0), "meta due");
    for (int s = -3; s <= 3; ++s) check(fg381s_decide(2, s) == (s < 0 ? 1 : 0), "drop all");
    std::printf("FG381S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
