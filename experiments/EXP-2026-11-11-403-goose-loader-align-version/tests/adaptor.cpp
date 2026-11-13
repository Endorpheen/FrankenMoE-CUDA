#include <cstdint>
#include <cstdio>
#include <initializer_list>
extern "C" int64_t fg403s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg403s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg403s_enabled() == 1, "enabled");
    int64_t o2;
    for (int64_t align : {1, 4, 32, 64})
        for (int64_t off = 0; off <= 300; off += 3) {
            check(fg403s_decide(0, 0, off, align, 0, 0, &o2) == 1, "no mmap always ok");
            check(fg403s_decide(0, 1, off, align, 0, 0, &o2) == ((off % align == 0) ? 1 : 0), "mmap alignment");
        }
    std::printf("FG403S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
