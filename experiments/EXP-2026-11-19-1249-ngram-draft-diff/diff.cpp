// Differential for the prompt-lookup drafter: bmoe::ngram_draft (real, linked) against the ported
// door sdp_ngram_draft in libllama (loaded at run time). Random contexts, exact output comparison.
#include <dlfcn.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>
#include "core/include/bmoe/ngram_draft.h"

struct sl_i32 { int32_t * data; int64_t len; };
typedef int64_t (*fn_draft)(sl_i32, int32_t, int64_t, int64_t, int64_t, sl_i32);

int main() {
    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto port = (fn_draft) dlsym(h, "sdp_ngram_draft");
    if (!port) { printf("missing door sdp_ngram_draft\n"); return 3; }

    std::mt19937 rng(4242);
    int fails = 0, cases = 0;
    for (int t = 0; t < 3000; ++t) {
        const int n = 4 + (int)(rng() % 40);
        std::vector<int32_t> ctx(n);
        for (auto & v : ctx) v = (int32_t)(rng() % 5);       // small alphabet: repeats are likely
        const int32_t last = ctx.back();
        const int n_max = 1 + (int)(rng() % 8);
        const int min_match = 1 + (int)(rng() % 3);
        const int max_match = min_match + (int)(rng() % 4);

        std::vector<int32_t> want;
        const int w = bmoe::ngram_draft(ctx, last, n_max, min_match, max_match, want);

        std::vector<int32_t> got_buf(64, -1);
        sl_i32 sin{ ctx.data(), (int64_t) ctx.size() };
        sl_i32 sout{ got_buf.data(), (int64_t) got_buf.size() };
        const int64_t g = port(sin, last, n_max, min_match, max_match, sout);

        ++cases;
        bool ok = (int64_t) w == g && (int64_t) want.size() == g;
        if (ok) ok = std::memcmp(want.data(), got_buf.data(), (size_t) g * sizeof(int32_t)) == 0;
        if (!ok) {
            ++fails;
            if (fails <= 3) {
                printf("MISMATCH n=%d n_max=%d min=%d max=%d native=%d port=%lld\n",
                       n, n_max, min_match, max_match, w, (long long) g);
            }
        }
    }
    printf("ngram draft differential: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
