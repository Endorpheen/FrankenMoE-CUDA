#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
extern "C" int64_t fg384s_softmax(const float * logits, int64_t n, int32_t sorted, float * out);
extern "C" int fg384s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
static uint32_t bits(float v) { uint32_t u; std::memcpy(&u, &v, 4); return u; }
// native transcription: llama_sampler_softmax_impl with do_sort = false
static void ref(const float * l, int n, bool sorted, float * out) {
    float max_l = l[0];
    if (!sorted) for (int i = 1; i < n; ++i) max_l = std::max(max_l, l[i]);
    float cum = 0.0f;
    for (int i = 0; i < n; ++i) { out[i] = expf(l[i] - max_l); cum += out[i]; }
    for (int i = 0; i < n; ++i) out[i] /= cum;
}
int main(void) {
    check(fg384s_enabled() == 1, "enabled");
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> unif(-30.0f, 30.0f);
    int sets = 0, cmp = 0;
    for (int n = 1; n <= 64; n = n * 2 + (n % 3)) {
        for (int trial = 0; trial < 40; ++trial) {
            std::vector<float> l(n), got(n, -7.0f), want(n, -9.0f);
            for (auto & v : l) v = unif(rng);
            bool sorted = (trial % 2) == 1;
            if (sorted) std::sort(l.begin(), l.end(), std::greater<float>());
            fg384s_softmax(l.data(), n, sorted ? 1 : 0, got.data());
            ref(l.data(), n, sorted, want.data());
            for (int i = 0; i < n; ++i) { ++cmp; check(bits(got[i]) == bits(want[i]), "p bits"); }
            ++sets;
        }
    }
    // edge shapes: all equal, extreme spread, single element
    for (int trial = 0; trial < 3; ++trial) {
        std::vector<float> l = {0.0f, 0.0f, 0.0f, 0.0f}, got(4), want(4);
        if (trial == 1) l = {-88.0f, 0.0f, 88.0f, 1e-30f};
        if (trial == 2) l = {3.0f};
        got.assign(l.size(), -1.0f); want.assign(l.size(), -2.0f);
        fg384s_softmax(l.data(), (int64_t) l.size(), 0, got.data());
        ref(l.data(), (int) l.size(), false, want.data());
        for (size_t i = 0; i < l.size(); ++i) { ++cmp; check(bits(got[i]) == bits(want[i]), "edge bits"); }
        ++sets;
    }
    std::printf("FG384S_ADAPTOR sets=%d comparisons=%d failures=%d\n", sets, cmp, failures);
    return failures ? 1 : 0;
}
