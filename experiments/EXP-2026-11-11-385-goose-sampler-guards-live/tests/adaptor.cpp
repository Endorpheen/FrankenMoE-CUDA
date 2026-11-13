#include <cstdint>
#include <cstdio>
extern "C" int64_t fg385s_decide(int64_t op, int64_t a, int64_t b);
extern "C" int fg385s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg385s_enabled() == 1, "enabled");
    const int64_t ps[6] = {-1, 0, 1, 500000, 999999, 1000000};
    for (int64_t p : ps)
        for (int64_t size = 0; size <= 2; ++size) {
            check(fg385s_decide(0, p, size) == ((p <= 0 || size == 0) ? 1 : 0), "min_p");
            check(fg385s_decide(1, p, 0) == (p >= 1000000 ? 1 : 0), "typical");
            check(fg385s_decide(2, p, 0) == (p >= 1000000 ? 1 : 0), "top_p");
            check(fg385s_decide(3, p, 0) == (p <= 0 ? 1 : 0), "top_k");
        }
    std::printf("FG385S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
