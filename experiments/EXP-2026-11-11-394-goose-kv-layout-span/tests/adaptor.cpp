#include <cstdint>
#include <cstdio>
#include <initializer_list>
extern "C" int64_t fg394s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg394s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg394s_enabled() == 1, "enabled");
    int64_t size;
    for (int64_t first = 0; first <= 8; first += 4)
        for (int64_t count = 0; count <= 8; count += 4)
            for (int64_t row : {1, 4, 64}) {
                check(fg394s_decide(0, first, count, row, 0, 0, &size) == first * row, "row offset");
                check(size == count * row, "row size");
                for (int64_t j = 0; j <= 4; ++j)
                    for (int64_t csize : {1, 16, 128}) {
                        check(fg394s_decide(1, first, count, row, j, csize, &size) == (first + j * csize) * row, "col offset");
                        check(size == count * row, "col size");
                    }
            }
    for (int64_t n = 0; n <= 9; ++n) check(fg394s_decide(2, n, 0, 0, 0, 0, &size) == n && size == n, "rows");
    std::printf("FG394S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
