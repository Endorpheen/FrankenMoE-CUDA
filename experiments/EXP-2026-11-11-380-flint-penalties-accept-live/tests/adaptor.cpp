#include <cstdint>
#include <cstdio>
extern "C" int64_t fg380s_decide(int64_t op, int64_t a, int64_t b);
extern "C" int fg380s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg380s_enabled() == 1, "enabled");
    for (int window = 0; window <= 4; ++window) check(fg380s_decide(0, 7, window) == (window == 0 ? 1 : 0), "skip");
    for (int size = 0; size <= 8; ++size)
        for (int window = 0; window <= 8; ++window)
            check(fg380s_decide(1, size, window) == (size >= window && window > 0 ? 1 : 0) || window == 0, "evict");
    for (int c = -1; c <= 3; ++c) check(fg380s_decide(2, c, 0) == (c <= 0 ? 1 : 0), "drop");
    std::printf("FG380S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
