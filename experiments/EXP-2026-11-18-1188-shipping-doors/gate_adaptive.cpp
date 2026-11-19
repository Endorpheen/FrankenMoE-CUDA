// Adaptive-P differential: the shipping door against the native formula in llama-sampler.cpp.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>
#include <algorithm>
extern "C" int64_t fg_sampler_adaptive_p_call(float *, float *, int64_t *, int64_t, float, float, float);
static constexpr float PEAK = 5.0f, SHARP = 10.0f, INVW = 1.0f / 0.3f;
int main() {
    std::mt19937 rng(2026);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    int fails = 0, cases = 0;
    for (int it = 0; it < 1000; ++it) {
        int n = 1 + (int)(rng() % 64);
        std::vector<float> lv(n), p(n);
        std::vector<int64_t> id(n);
        for (int i = 0; i < n; ++i) { lv[i] = 4.0f * u(rng) - 2.0f; id[i] = i; }
        // softmax
        float mx = *std::max_element(lv.begin(), lv.end()), s = 0.0f;
        for (int i = 0; i < n; ++i) { p[i] = expf(lv[i] - mx); s += p[i]; }
        for (int i = 0; i < n; ++i) p[i] /= s;
        float target = 2.0f * u(rng) - 0.5f;
        float ws = u(rng) * 3.0f;
        float tw = (rng() % 4 == 0) ? 0.0f : u(rng) * 4.0f;
        std::vector<float> pl = lv, pp = p;
        int64_t rc = fg_sampler_adaptive_p_call(pl.data(), pp.data(), id.data(), n, target, ws, tw);
        if (rc != n) { fails++; if (fails <= 3) printf("RET MISMATCH n=%d rc=%lld\n", n, (long long) rc); continue; }
        float t = std::min(1.0f, std::max(0.0f, target));
        float adapted = (tw == 0.0f) ? t : 2.0f * t - (ws / tw);
        adapted = std::min(1.0f, std::max(0.0f, adapted));
        for (int i = 0; i < n; ++i) {
            float d = fabsf((p[i] - adapted) * INVW);
            float want = PEAK - SHARP * d * d / (1.0f + d);
            cases++;
            if (fabsf(pl[i] - want) > 1e-4f) { fails++; if (fails <= 3) printf("MISMATCH i=%d goose=%g want=%g\n", i, (double) pl[i], (double) want); break; }
        }
    }
    printf("adaptive-p: cases=%d mismatches=%d\n", cases, fails);
    return fails ? 1 : 0;
}
