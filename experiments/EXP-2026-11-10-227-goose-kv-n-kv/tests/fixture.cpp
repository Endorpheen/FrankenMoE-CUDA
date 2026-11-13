// Differential gate for llama_kv_cache::get_n_kv against a transcription of the native body.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>
#include <algorithm>
extern "C" { int64_t fg227n_n_kv(int64_t, const int32_t *, const int32_t *, int64_t); }

static uint32_t ggml_pad_ref(uint32_t x, uint32_t n) { return (x + n - 1) / n * n; }

static uint32_t ref_n_kv(const std::vector<int32_t>& sizes, const std::vector<int32_t>& used, uint32_t n_pad) {
    uint32_t result = 0;
    const uint32_t n_pad_cur = std::max<uint32_t>(n_pad, 256u);
    for (size_t s = 0; s < sizes.size(); ++s) {
        const uint32_t csize = (uint32_t) sizes[s];
        const uint32_t umax  = (uint32_t) used[s];
        result = std::max(std::min(csize, std::max(n_pad_cur, ggml_pad_ref(umax, n_pad_cur))), result);
    }
    return result;
}

static int checks, failures;
static void ck(bool ok, int64_t a, int64_t b, const char * w) {
    ++checks;
    if (!ok) { if (failures < 15) printf("FAIL %s goose=%lld ref=%lld\n", w, (long long)a, (long long)b); ++failures; }
}

int main(void) {
    std::mt19937 rng(227011);
    // edge cases first: empty, all-free cells, huge n_pad, small n_pad, used beyond capacity
    struct { std::vector<int32_t> sz, us; uint32_t npad; } fixed[] = {
        {{}, {}, 0},
        {{0}, {0}, 0},
        {{100000}, {0}, 0},
        {{100000}, {5}, 0},
        {{100000}, {5}, 1},
        {{100000}, {5}, 256},
        {{100000}, {100000}, 0},
        {{10}, {100000}, 0},
        {{512, 512}, {300, 100}, 0},
        {{0, 4096}, {0, 4095}, 512},
        {{4096}, {4096}, 4096},
    };
    size_t idx = 0;
    for (auto & c : fixed) {
        const int64_t got = fg227n_n_kv((int64_t) c.sz.size(), c.sz.empty() ? nullptr : c.sz.data(),
                                        c.us.empty() ? nullptr : c.us.data(), (int64_t) c.npad);
        ck(got == (int64_t) ref_n_kv(c.sz, c.us, c.npad), got, (long long) ref_n_kv(c.sz, c.us, c.npad), "fixed");
        ++idx;
    }
    // random shapes
    for (int t = 0; t < 5000; ++t) {
        const int n = (int) (rng() % 5);
        std::vector<int32_t> sz(n), us(n);
        for (int i = 0; i < n; ++i) { sz[i] = (int32_t) (rng() % 70000); us[i] = (int32_t) (rng() % 70000); }
        const uint32_t npad = (rng() % 3 == 0) ? (uint32_t) (rng() % 8192) : (uint32_t) (rng() % 300);
        const int64_t got = fg227n_n_kv(n, n ? sz.data() : nullptr, n ? us.data() : nullptr, (int64_t) npad);
        ck(got == (int64_t) ref_n_kv(sz, us, npad), got, (long long) ref_n_kv(sz, us, npad), "rand");
    }
    printf("FG227N_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP227_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP227_LOCAL_GATES_PASS\n");
    return 0;
}
