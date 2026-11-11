#include <cstdint>
#include <cstdio>
extern "C" int64_t fg383s_decide(int64_t op, int64_t a, int64_t b);
extern "C" int fg383s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg383s_enabled() == 1, "enabled");
    for (int mixed = 0; mixed <= 1; ++mixed)
        for (int n = 0; n <= 5; ++n) {
            check(fg383s_decide(0, mixed, n) == (mixed ? n : 0), "type cap");
            check(fg383s_decide(1, mixed, n) == (mixed ? 0 : n), "order cap");
            check(fg383s_decide(2, mixed, n) == n, "seqid cap");
        }
    std::printf("FG383S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
