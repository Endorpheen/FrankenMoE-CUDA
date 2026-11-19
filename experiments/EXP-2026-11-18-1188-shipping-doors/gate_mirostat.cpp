// Mirostat-k differential: the shipping door against the native formula, with the same
// integer-rounded k the native code uses.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>
#include <algorithm>
extern "C" int64_t fg_sampler_mirostat_call(float *, int64_t, float, int64_t, float);
int main() {
    std::mt19937 rng(77);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    int fails = 0, cases = 0;
    for (int it = 0; it < 500; ++it) {
        int n = 8 + (int)(rng() % 120);
        std::vector<float> lv(n), p(n);
        for (int i = 0; i < n; ++i) lv[i] = 3.0f * u(rng) - 1.5f;
        float mx = *std::max_element(lv.begin(), lv.end()), s = 0.0f;
        for (int i = 0; i < n; ++i) { p[i] = expf(lv[i] - mx); s += p[i]; }
        for (int i = 0; i < n; ++i) p[i] /= s;
        // the native sorts descending before this point
        std::sort(p.begin(), p.end(), [](float a, float b){ return a > b; });
        float mu = 0.5f + 3.0f * u(rng);
        int m = 4 + (int)(rng() % 20);
        float nv = 32000.0f + u(rng) * 100000.0f;
        float sum_ti_bi = 0, sum_ti_sq = 0;
        for (int i = 0; i < m - 1 && i < n - 1; ++i) {
            float t_i = logf((float)(i + 2) / (float)(i + 1));
            float b_i = logf(p[i] / p[i + 1]);
            sum_ti_bi += t_i * b_i; sum_ti_sq += t_i * t_i;
        }
        float s_hat = sum_ti_bi / sum_ti_sq;
        float eps = s_hat - 1;
        float kf = powf((eps * powf(2.0f, mu)) / (1.0f - powf(nv, -eps)), 1.0f / s_hat);
        // the native applies std::max(int(k), 1): truncation toward zero, then a floor of 1
        int64_t want = (int64_t) kf; if (want < 1) want = 1;
        std::vector<float> pp = p;
        int64_t got = fg_sampler_mirostat_call(pp.data(), n, mu, m, nv);
        cases++;
        if (got != want) { fails++; if (fails <= 5) printf("MISMATCH n=%d mu=%g m=%d got=%lld want=%lld (kf=%g)\n",
            n, (double) mu, m, (long long) got, (long long) want, (double) kf); }
    }
    printf("mirostat-k: cases=%d mismatches=%d\n", cases, fails);
    return fails ? 1 : 0;
}
