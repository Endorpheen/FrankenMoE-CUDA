// RNG stream: the shipping dist door against std::mt19937 with the canonical double draw.
// The door consumes two words per draw, exactly like the native sampler, so comparing a
// sequence also proves the stream position advances identically.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>
extern "C" int64_t fg_sampler_dist_call(float *, int64_t, int64_t *);
extern "C" int64_t fg_sampler_chain_call(int64_t, const float *, float *, int64_t *, int64_t,
                                         float, int64_t, float, float);
// The module seeds through its own door; hand-filling the state with mt() would use a
// different expansion than std::mt19937's seeding, which is what my first attempt did.
static void seed_state(int64_t seed, int64_t * st) {
    memset(st, 0, 625 * sizeof(int64_t));
    static float lv[1] = {0}, pp[1] = {0};
    static int64_t id[1] = {0};
    fg_sampler_chain_call(10, lv, pp, id, 1, 0.0f, seed, 0.0f, 0.0f);
    // the door cannot write our buffer, so reproduce its recurrence here (documented in the
    // module as std::mt19937's seeding) and let the draws be compared instead
    st[0] = seed & 0xFFFFFFFFLL;
    for (int i = 1; i < 624; ++i) {
        int64_t prev = st[i - 1];
        st[i] = (1812433253LL * (prev ^ (prev >> 30)) + i) & 0xFFFFFFFFLL;
    }
    st[624] = 624;
}
int main() {
    int fails = 0, cases = 0;
    const uint32_t seeds[] = {1u, 42u, 12345u, 7777777u};
    for (uint32_t seed : seeds) {
        // ON path: the module advances a copied state
        std::vector<int64_t> st(625);
        seed_state((int64_t) seed, st.data());
        // OFF path: the native generator with the same seeding
        std::mt19937 mt_off(seed);
        std::vector<float> p = {0.25f, 0.25f, 0.25f, 0.25f};
        for (int k = 0; k < 320; ++k) {
            int64_t sel = fg_sampler_dist_call(p.data(), 4, st.data());
            // the canonical double of this project: low word plus high word, over 2^64
            const double r1 = (double) (uint32_t) mt_off();
            const double r2 = (double) (uint32_t) mt_off();
            double u = (r1 + r2 * 4294967296.0) / 18446744073709551616.0;
            int native = (int)(u * 4.0);
            if (native > 3) native = 3;
            cases++;
            if (sel != native) {
                fails++;
                if (fails <= 5) printf("STREAM MISMATCH seed=%u draw=%d goose=%lld native=%d (u=%.17g)\n",
                                       seed, k, (long long) sel, native, u);
            }
        }
    }
    printf("rng stream: cases=%d mismatches=%d\n", cases, fails);
    return fails ? 1 : 0;
}
