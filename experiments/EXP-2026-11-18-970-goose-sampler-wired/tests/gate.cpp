// EXP970 differential gate: with Goose wired into the native typical filter, the
// shipping sampler must still produce exactly what the accepted library produces.
//
// This is the acceptance question for the wiring itself, and it needs no model:
// construct llama_sampler_init_typical from each library, apply it to the same
// candidate array, and compare the survivors and the written probabilities.
//
// Both arms of GGML_GOOSE_SAMPLER are exercised: the run is repeated with the
// variable set to 0, which must reproduce the native path exactly.
//
// Usage: gate <candidate-libllama.so.0> <accepted-libllama.so.0>
//   The two libraries are loaded with dlopen so both can be in one process.
#include <dlfcn.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <random>
#include <string>
#include <vector>

// llama.h types, repeated so the gate does not need the header twice.
typedef int32_t g_token;
typedef struct { g_token id; float logit; float p; } g_data;
typedef struct { g_data * data; size_t size; int64_t selected; bool sorted; } g_array;
struct g_sampler;

typedef struct g_sampler * (*init_typical_fn)(float, size_t);
typedef void (*apply_fn)(struct g_sampler *, g_array *);
typedef void (*free_fn)(struct g_sampler *);

typedef void (*init_fn)(int, char **);
typedef int64_t (*calls_fn)(void);

static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what.c_str()); }
}
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, sizeof(u)); return u; }

struct Lib {
    void * h;
    init_typical_fn init;
    apply_fn apply;
    free_fn release;
    bool open(const char * path) {
        h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!h) { std::printf("dlopen %s: %s\n", path, dlerror()); return false; }
        // The composed unit's initialiser is renamed inside the DSO; call it the way
        // the reader owner does, or every exported function refuses.
        auto boot = (init_fn) dlsym(h, "fg950_goose_init");
        if (boot) boot(0, nullptr);
        init = (init_typical_fn) dlsym(h, "llama_sampler_init_typical");
        apply = (apply_fn) dlsym(h, "llama_sampler_apply");
        release = (free_fn) dlsym(h, "llama_sampler_free");
        return init && apply && release;
    }
};

// One run on one library; returns (survivor ids, their logits, their probabilities).
struct Result {
    std::vector<int64_t> ids;
    std::vector<float> logits, probs;
    size_t size = 0;
};

static Result run(Lib & lib, const std::vector<g_data> & seed, float p, size_t min_keep) {
    std::vector<g_data> arr = seed;
    g_array a{ arr.data(), arr.size(), -1, false };
    struct g_sampler * s = lib.init(p, min_keep);
    lib.apply(s, &a);
    Result r;
    r.size = a.size;
    for (size_t i = 0; i < a.size; i++) {
        r.ids.push_back(a.data[i].id);
        r.logits.push_back(a.data[i].logit);
        r.probs.push_back(a.data[i].p);
    }
    lib.release(s);
    return r;
}

int main(int argc, char ** argv) {
    if (argc < 3) { std::printf("usage: gate <candidate> <accepted>\n"); return 2; }

    Lib cand, acc;
    if (!cand.open(argv[1])) return 2;
    if (!acc.open(argv[2])) return 2;

    std::mt19937 rng(970);
    std::uniform_real_distribution<float> u(-6.0f, 6.0f);
    std::uniform_int_distribution<int> nsize(2, 40);

    for (int trial = 0; trial < 150; trial++) {
        const int n = nsize(rng);
        const float p = 0.1f + 0.85f * ((float) (rng() % 1000) / 1000.0f);
        const size_t mk = (size_t) (rng() % 3);

        std::vector<g_data> seed((size_t) n);
        for (int i = 0; i < n; i++) {
            seed[(size_t) i].id = i;
            seed[(size_t) i].logit = u(rng);
            seed[(size_t) i].p = 0.0f;
        }

        const Result a = run(cand, seed, p, mk);
        const Result b = run(acc, seed, p, mk);
        // witness is checked once, below, after all trials

        check(a.size == b.size, "survivor count");
        if (a.size != b.size) { std::printf("FAIL trial %d size %zu vs %zu\n", trial, a.size, b.size); continue; }

        // The surviving SET must match; the order inside a tie need not, because
        // native std::sort is unstable and the Goose filter breaks ties by index.
        std::vector<int64_t> ai = a.ids, bi = b.ids;
        std::sort(ai.begin(), ai.end());
        std::sort(bi.begin(), bi.end());
        checks++;
        if (ai != bi) {
            std::printf("FAIL trial %d survivor set differs\n", trial);
            failures++;
            continue;
        }
        // Every surviving entry must carry the same logit and probability bits,
        // whatever position the sort put it in.
        for (int64_t id : ai) {
            size_t ia = 0, ib = 0;
            for (size_t k = 0; k < a.ids.size(); k++) if (a.ids[k] == id) ia = k;
            for (size_t k = 0; k < b.ids.size(); k++) if (b.ids[k] == id) ib = k;
            checks++;
            if (bits(a.logits[ia]) != bits(b.logits[ib])) {
                std::printf("FAIL trial %d id %lld logit bits differ\n", trial, (long long) id);
                failures++;
            }
            checks++;
            if (bits(a.probs[ia]) != bits(b.probs[ib])) {
                std::printf("FAIL trial %d id %lld prob bits differ\n", trial, (long long) id);
                failures++;
            }
        }
    }

    // p >= 1.0 is the empty sampler on both sides.
    {
        std::vector<g_data> seed(4);
        for (int i = 0; i < 4; i++) { seed[(size_t) i].id = i; seed[(size_t) i].logit = (float) i; seed[(size_t) i].p = 0; }
        const Result a = run(cand, seed, 1.0f, 0);
        const Result b = run(acc, seed, 1.0f, 0);
        check(a.size == 4 && b.size == 4, "p=1.0 keeps everything on both");
    }

    // ---- positive witness: did the Goose hook actually run? ----
    {
        auto calls = (calls_fn) dlsym(cand.h, "goose_sampler_calls");
        check(calls != nullptr, "candidate exports the sampler witness");
        if (calls) {
            const int64_t n = calls();
            const char * off = std::getenv("GGML_GOOSE_SAMPLER");
            const bool disabled = off && std::strcmp(off, "0") == 0;
            std::printf("goose sampler calls=%lld%s\n", (long long) n, disabled ? " (flag off)" : "");
            if (disabled) check(n == 0, "flag off means the Goose hook never ran");
            else          check(n > 0, "flag on means the Goose hook ran");
        }
    }

    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
