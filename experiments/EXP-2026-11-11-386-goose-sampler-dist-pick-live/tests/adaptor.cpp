#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>
extern "C" int64_t fg386s_pick(const float * logits, int64_t n, int32_t sorted, float * out, double rnd);
extern "C" int fg386s_enabled(void);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }
static uint32_t bits(float v) { uint32_t u; std::memcpy(&u, &v, 4); return u; }

// transcription of llama_sampler_dist_apply (the #if 1 branch)
static int ref(const float * logit, int n, bool sorted, double rnd, float * p) {
    float max_l = logit[0];
    if (!sorted) for (int i = 1; i < n; ++i) max_l = std::max(max_l, logit[i]);
    double sum_cum = 0.0;
    for (int i = 0; i < n; ++i) { p[i] = expf(logit[i] - max_l); sum_cum += p[i]; }
    const double tgt = sum_cum * rnd;
    double run = 0.0;
    int sel = -1;
    for (int i = 0; i < n; ++i) {
        if (sel < 0) { run += p[i]; if (run >= tgt) sel = i; }
        p[i] = (float) ((double) p[i] / sum_cum);
    }
    return sel < 0 ? n - 1 : sel;
}

int main(void) {
    check(fg386s_enabled() == 1, "enabled");
    std::mt19937 rng(777);
    std::uniform_real_distribution<float> logit(-25.0f, 25.0f);
    std::uniform_real_distribution<double> draw(0.0, 1.0);
    int sets = 0, cmp = 0;
    for (int n = 1; n <= 300; n = n * 2 + (n % 5)) {
        for (int trial = 0; trial < 10; ++trial) {
            std::vector<float> l(n), got(n, 0.0f), want(n, 0.0f);
            for (auto & v : l) v = logit(rng);
            bool sorted = (trial % 2) == 1;
            if (sorted) std::sort(l.begin(), l.end(), std::greater<float>());
            for (int k = 0; k < 5; ++k) {
                const double u = draw(rng);
                const int a = (int) fg386s_pick(l.data(), n, sorted ? 1 : 0, got.data(), u);
                const int b = ref(l.data(), n, sorted, u, want.data());
                ++cmp;
                check(a == b, "selected index");
                for (int i = 0; i < n; ++i) { ++cmp; check(bits(got[i]) == bits(want[i]), "p bit"); }
            }
            ++sets;
        }
    }
    // boundary draws: exactly on a running-sum threshold
    for (int n = 2; n <= 64; n *= 2) {
        std::vector<float> l(n), got(n), want(n);
        for (int i = 0; i < n; ++i) l[i] = (float) (i + 1) / 10.0f;
        float max_l = *std::max_element(l.begin(), l.end());
        double sc = 0.0;
        for (int i = 0; i < n; ++i) sc += expf(l[i] - max_l);
        double run = 0.0;
        for (int i = 0; i < n; ++i) {
            run += expf(l[i] - max_l);
            const double u = run / sc;                    // lands exactly on this threshold
            const int a = (int) fg386s_pick(l.data(), n, 0, got.data(), u);
            const int b = ref(l.data(), n, false, u, want.data());
            ++cmp; check(a == b, "boundary index");
            for (int j = 0; j < n; ++j) { ++cmp; check(bits(got[j]) == bits(want[j]), "boundary p bit"); }
        }
    }
    std::printf("FG386S_ADAPTOR sets=%d comparisons=%d failures=%d\n", sets, cmp, failures);
    return failures ? 1 : 0;
}
