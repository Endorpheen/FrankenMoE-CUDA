// EXP971 differential gate: with all three filters wired, each shipping sampler
// must still produce exactly what the accepted library produces.
//
// Each arm is a real llama_sampler from each library, applied to the same input:
//   typical      - candidate array, compare survivor sets and probability bits
//   top-n-sigma  - candidate array, compare every logit bit
//   dry          - history fed through accept, compare every logit bit
//
// The witness says which path decided, so identical output plus a non-zero count
// proves Goose ran rather than the native body.
//
// Usage: gate <candidate-libllama.so.0> <accepted-libllama.so.0>
#include <dlfcn.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <vector>

typedef int32_t g_token;
typedef struct { g_token id; float logit; float p; } g_data;
typedef struct { g_data * data; size_t size; int64_t selected; bool sorted; } g_array;
struct g_sampler;

static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what.c_str()); }
}
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, sizeof(u)); return u; }

// llama_sampler_init_dry_testing, matching src/llama-sampler.h:41.
typedef struct g_sampler * (*dry_testing_fn)(float, float, int32_t, int32_t,
                                             const std::vector<std::vector<g_token>> &);

struct Lib {
    void * h = nullptr;
    void (*boot)(int, char **) = nullptr;
    struct g_sampler * (*init_typical)(float, size_t) = nullptr;
    struct g_sampler * (*init_tns)(float) = nullptr;
    dry_testing_fn init_dry = nullptr;
    void (*apply)(struct g_sampler *, g_array *) = nullptr;
    void (*accept)(struct g_sampler *, g_token) = nullptr;
    void (*release)(struct g_sampler *) = nullptr;
    int64_t (*calls)(void) = nullptr;

    bool open(const char * path) {
        h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!h) { std::printf("dlopen %s: %s\n", path, dlerror()); return false; }
        boot = (decltype(boot)) dlsym(h, "fg950_goose_init");
        if (boot) boot(0, nullptr);
        init_typical = (decltype(init_typical)) dlsym(h, "llama_sampler_init_typical");
        init_tns = (decltype(init_tns)) dlsym(h, "llama_sampler_init_top_n_sigma");
        // A C++ name, so it is mangled; the declaration below resolves it at link
        // time against whichever library this object was linked with. When the
        // candidate and the accepted library are both loaded, the dynamic linker
        // picks the first definition - so the DRY arm is run once per library in
        // separate processes instead of by dlsym (see the README).
        init_dry = (decltype(init_dry)) dlsym(h, "_Z30llama_sampler_init_dry_testingffiiRKSt6vectorIS_IiSaIiEESaIS1_EE");
        apply = (decltype(apply)) dlsym(h, "llama_sampler_apply");
        accept = (decltype(accept)) dlsym(h, "llama_sampler_accept");
        release = (decltype(release)) dlsym(h, "llama_sampler_free");
        calls = (decltype(calls)) dlsym(h, "goose_sampler_calls");
        return init_typical && init_tns && init_dry && apply && accept && release;
    }
};

static std::vector<g_data> make_candidates(std::mt19937 & rng, int n) {
    std::uniform_real_distribution<float> u(-6.0f, 6.0f);
    std::vector<g_data> c((size_t) n);
    for (int i = 0; i < n; i++) { c[(size_t) i].id = i; c[(size_t) i].logit = u(rng); c[(size_t) i].p = 0; }
    return c;
}

int main(int argc, char ** argv) {
    if (argc < 3) { std::printf("usage: gate <candidate> <accepted>\n"); return 2; }
    Lib cand, acc;
    if (!cand.open(argv[1])) return 2;
    if (!acc.open(argv[2])) return 2;

    std::mt19937 rng(971);
    std::uniform_int_distribution<int> nsize(2, 40);

    // ---- typical ----
    for (int trial = 0; trial < 100; trial++) {
        const int n = nsize(rng);
        const float p = 0.1f + 0.85f * ((float) (rng() % 1000) / 1000.0f);
        const size_t mk = (size_t) (rng() % 3);
        auto seed = make_candidates(rng, n);

        auto run = [&](Lib & L) {
            std::vector<g_data> a = seed;
            g_array arr{ a.data(), a.size(), -1, false };
            struct g_sampler * s = L.init_typical(p, mk);
            L.apply(s, &arr);
            std::vector<int64_t> ids;
            std::vector<float> probs;
            for (size_t i = 0; i < arr.size; i++) { ids.push_back(arr.data[i].id); probs.push_back(arr.data[i].p); }
            L.release(s);
            return std::make_pair(ids, probs);
        };
        auto a = run(cand), b = run(acc);
        check(a.first.size() == b.first.size(), "typical survivor count");
        if (a.first.size() != b.first.size()) continue;
        auto sa = a.first, sb = b.first;
        std::sort(sa.begin(), sa.end()); std::sort(sb.begin(), sb.end());
        check(sa == sb, "typical survivor set");
        // probabilities per id
        for (size_t k = 0; k < sa.size(); k++) {
            float pa = 0, pb = 0;
            for (size_t j = 0; j < a.first.size(); j++) if (a.first[j] == sa[k]) pa = a.second[j];
            for (size_t j = 0; j < b.first.size(); j++) if (b.first[j] == sa[k]) pb = b.second[j];
            check(bits(pa) == bits(pb), "typical probability bits");
        }
    }

    // ---- top-n-sigma ----
    for (int trial = 0; trial < 100; trial++) {
        const int n = nsize(rng);
        const float sigma = 0.25f + 3.5f * ((float) (rng() % 1000) / 1000.0f);
        auto seed = make_candidates(rng, n);
        auto run = [&](Lib & L) {
            std::vector<g_data> a = seed;
            g_array arr{ a.data(), a.size(), -1, false };
            struct g_sampler * s = L.init_tns(sigma);
            L.apply(s, &arr);
            std::vector<float> out;
            for (size_t i = 0; i < arr.size; i++) out.push_back(arr.data[i].logit);
            L.release(s);
            return out;
        };
        auto a = run(cand), b = run(acc);
        check(a.size() == b.size(), "tns size unchanged");
        for (size_t i = 0; i < a.size() && i < b.size(); i++) check(bits(a[i]) == bits(b[i]), "tns logit bits");
    }

    // ---- DRY ----
    for (int trial = 0; trial < 100; trial++) {
        const int n = nsize(rng);
        auto seed = make_candidates(rng, n);
        std::uniform_int_distribution<int> tok(0, 6);
        std::vector<g_token> hist((size_t) (5 + trial % 20));
        for (auto & t : hist) t = tok(rng);
        std::vector<std::vector<g_token>> breakers;
        if (trial % 2 == 0) breakers.push_back({ 13 });
        const float mult = 0.4f + 0.15f * (float) (trial % 5);
        const float base = (trial % 3 == 0) ? 1000.0f : 1.75f;

        auto run = [&](Lib & L) {
            std::vector<g_data> a = seed;
            struct g_sampler * s = L.init_dry(mult, base, 2, 64, breakers);
            for (auto t : hist) L.accept(s, t);
            g_array arr{ a.data(), a.size(), -1, false };
            L.apply(s, &arr);
            std::vector<float> out;
            for (size_t i = 0; i < arr.size; i++) out.push_back(arr.data[i].logit);
            L.release(s);
            return out;
        };
        auto a = run(cand), b = run(acc);
        check(a.size() == b.size(), "dry size unchanged");
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            checks++;
            if (bits(a[i]) != bits(b[i])) {
                std::printf("FAIL dry trial %d id %zu: %g vs %g\n", trial, i, a[i], b[i]);
                failures++;
            }
        }
    }

    // ---- witness ----
    if (cand.calls) {
        const int64_t n = cand.calls();
        const char * off = std::getenv("GGML_GOOSE_SAMPLER");
        const bool disabled = off && std::strcmp(off, "0") == 0;
        std::printf("goose sampler calls=%lld%s\n", (long long) n, disabled ? " (flag off)" : "");
        if (disabled) check(n == 0, "flag off: the Goose hook never ran");
        else          check(n > 0, "flag on: the Goose hook ran");
    } else {
        check(false, "candidate exports the witness");
    }

    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
