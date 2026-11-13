#include <cstdint>
#include <cstdio>
#include <cstdlib>
extern "C" int64_t fg373s_guard(int64_t op, int64_t a, int64_t b, int64_t c);
extern "C" int fg373s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * what) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", what); } }
int main(void) {
    check(fg373s_enabled() == 1, "enabled");
    for (int mirror = 0; mirror <= 1; ++mirror)
        for (int size = 0; size <= 3; ++size)
            for (int ns = 1; ns <= 3; ++ns) {
                const bool bad = mirror && size != ns;
                check((fg373s_guard(0, mirror, size, ns) < 0) == bad, "mirror size");
            }
    for (int cur = 0; cur <= 3; ++cur)
        for (int ns = 1; ns <= 3; ++ns)
            check((fg373s_guard(1, cur, ns, 0) < 0) == (cur != ns), "stream count");
    for (int cc = 0; cc <= 1; ++cc)
        for (int empty = 0; empty <= 1; ++empty)
            check((fg373s_guard(2, cc, empty, 0) < 0) == (cc == 0 && !empty), "zero-cell mirror");
    std::printf("FG373S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
