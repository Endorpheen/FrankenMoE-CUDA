#include <cstdint>
#include <cstdio>
#include <initializer_list>
extern "C" int64_t fg406s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);
extern "C" int fg406s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
int main(void) {
    check(fg406s_enabled() == 1, "enabled");
    int64_t o2;
    for (int64_t n : {(int64_t) -1, (int64_t) 0, (int64_t) 1, (int64_t) 4096})
        check(fg406s_decide(0, n, 0, 0, 0, 0, &o2) == (n > 0 ? 1 : 0), "async");
    for (int l = 0; l <= 1; ++l)
        for (int e = 0; e <= 1; ++e) {
            const int64_t want = l ? 0 : (e ? 1 : 2);
            check(fg406s_decide(1, l, e, 0, 0, 0, &o2) == want, "kind");
        }
    for (int64_t end = 0; end <= 10; end += 5)
        for (int64_t pos = 0; pos <= 10; pos += 5)
            check(fg406s_decide(2, end, pos, 0, 0, 0, &o2) == end - pos, "chunk");
    std::printf("FG406S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
