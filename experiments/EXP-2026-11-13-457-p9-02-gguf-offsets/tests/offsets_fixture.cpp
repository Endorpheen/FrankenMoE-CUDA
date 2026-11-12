// Differential fixture for the Goose GGUF offset indexing (EXP457).
//
// The expected table comes from gen_expected.py, which parses the Apex GGUF with
// struct, reads the type table from the gguf library and cross-checks every
// offset against GGUFReader.data_offset before this fixture runs. The fixture
// recomputes the layout with the Goose module and compares.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#include "expected_layout.inc"

#include "ffi-adapter.h"

int64_t fg457_ld_i64(uint64_t p, uint64_t o) {
    return *(const int64_t *) ((const char *) (uintptr_t) p + o);
}

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const char * detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail);
}

static int64_t native_up(int64_t x, int64_t n) { return (x + n - 1) & ~(n - 1); }

int main(void) {
    goose_init(0, 0);
    // 1. alignment must be a non-zero power of two
    for (int64_t a = -2; a <= 129; ++a) {
        int64_t got = fg457_ok(a);
        int64_t want = (a > 0 && (a & (a - 1)) == 0) ? 1 : 0;
        ++cases; ++checks;
        if (got != want) fail("align_ok", std::to_string(a).c_str());
    }

    // 2. round-up matches GGML_PAD over a grid
    const int64_t aligns[] = {1, 2, 4, 32, 64, 4096};
    for (int64_t n : aligns)
    for (int64_t x = 0; x <= 200; ++x) {
        ++cases; ++checks;
        if (fg457_up(x, n) != native_up(x, n)) fail("align_up", std::to_string(x).c_str());
    }

    // 3. byte size: type_size * (nelements / block), with divisibility enforced
    struct { int64_t t, b, ne, want; } nb_cases[] = {
        {144, 256, 256, 144}, {210, 256, 256, 210}, {4, 1, 1000, 4000},
        {144, 256, 512, 288}, {2, 1, 0, 0}, {144, 256, 100, -3}, {144, 0, 256, -3},
        {144, 256, -256, -3},
    };
    for (const auto & c : nb_cases) {
        int64_t out[1] = {-99};
        int64_t got = fg457_nbytes(c.t, c.b, c.ne, out);
        ++cases; ++checks;
        if (got != c.want || out[0] != c.want) fail("nbytes", "case");
    }

    // 4. the real file: data section start is structural end rounded up
    ++cases; ++checks;
    if (fg457_up(EXP457_STRUCTURAL_END, EXP457_ALIGN) != EXP457_DATA_START)
        fail("data_start", "align_up(structural_end) != data_start");

    // 5. full layout over all 753 tensors
    std::vector<int64_t> out((size_t) EXP457_N + 1, -1);
    int64_t status = fg457_layout(REL_OFFSETS, NBYTES, EXP457_N, EXP457_ALIGN, EXP457_DATA_START, out.data());
    ++cases; ++checks;
    if (status != 0) fail("layout-status", std::to_string(status).c_str());
    for (int i = 0; i < EXP457_N; ++i) {
        ++cases; ++checks;
        if (out[(size_t) i] != ABS_OFFSETS[i]) {
            char d[128];
            snprintf(d, sizeof d, "tensor %d goose=%lld expected=%lld", i,
                     (long long) out[(size_t) i], (long long) ABS_OFFSETS[i]);
            fail("abs-offset", d);
        }
    }
    ++cases; ++checks;
    if (out[(size_t) EXP457_N] != EXP457_TOTAL) {
        char d[128];
        snprintf(d, sizeof d, "total goose=%lld expected=%lld",
                 (long long) out[(size_t) EXP457_N], (long long) EXP457_TOTAL);
        fail("total-size", d);
    }

    // 6. broken inputs are rejected, not silently accepted
    {
        int64_t rel[3] = {0, 64, 200};      // 200 != padded end of the second tensor
        int64_t nb[3] = {10, 10, 10};
        std::vector<int64_t> o(4, -1);
        ++cases; ++checks;
        if (fg457_layout(rel, nb, 3, 32, 1000, o.data()) != -2) fail("layout-gap", "expected -2");

        int64_t rel2[2] = {0, 32};
        int64_t nb2[2] = {10, 10};
        ++cases; ++checks;
        if (fg457_layout(rel2, nb2, 2, 24, 1000, o.data()) != -1) fail("layout-align", "expected -1");
    }

    // 7. synthetic layout with sizes that are not multiples of the alignment:
    // real Apex tensors are all 32-aligned, so without this case the padding of
    // the total size would be untested.
    {
        int64_t rel[3] = {0, 32, 64};
        int64_t nb[3] = {10, 10, 10};
        std::vector<int64_t> o(4, -1);
        ++cases; ++checks;
        if (fg457_layout(rel, nb, 3, 32, 1000, o.data()) != 0) fail("synthetic-status", "expected 0");
        const int64_t want_abs[3] = {1000, 1032, 1064};
        for (int i = 0; i < 3; ++i) {
            ++cases; ++checks;
            if (o[(size_t) i] != want_abs[i]) fail("synthetic-abs", "offset");
        }
        ++cases; ++checks;
        if (o[3] != 96) fail("synthetic-total", "padded total must be 96, not 30");
    }

    printf("FG457_OFFSETS tensors=%d cases=%d values=%d failures=%d\n",
           EXP457_N, cases, checks, fails);
    return fails ? 1 : 0;
}
