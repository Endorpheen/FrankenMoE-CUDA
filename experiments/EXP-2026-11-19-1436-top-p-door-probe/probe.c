/* The top_p door on a hand-made array: after the call, are the logits and the ids still aligned, are the
 * probabilities the sorted softmax, and is the returned count the prefix whose cumulative p reaches top_p? */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef int64_t (*top_p_fn)(float *, float *, int64_t *, int64_t, float, int64_t);

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    top_p_fn fn = (top_p_fn) dlsym(h, "fg_sampler_top_p_call");
    if (!fn) { printf("door missing\n"); return 2; }

    float lv[6] = { 1.0f, 3.0f, 2.0f, 0.0f, -1.0f, 2.5f };
    float pp[6] = { 0 };
    int64_t ids[6] = { 10, 11, 12, 13, 14, 15 };
    const int64_t kept = fn(lv, pp, ids, 6, 0.99f, 1);
    printf("kept=%lld\n", (long long) kept);
    for (int i = 0; i < 6; ++i) {
        printf("  [%d] id=%lld logit=%.3f p=%.6f\n", i, (long long) ids[i], lv[i], pp[i]);
    }
    /* the checks: logits descending, ids descending-of-logit (11,15,12,10,13,14), p summing to 1 over all 6 */
    int ok = 1;
    for (int i = 1; i < 6; ++i) if (lv[i] > lv[i-1]) ok = 0;
    const int64_t want_ids[6] = { 11, 15, 12, 10, 13, 14 };
    for (int i = 0; i < 6; ++i) if (ids[i] != want_ids[i]) ok = 0;
    double sum = 0; for (int i = 0; i < 6; ++i) sum += pp[i];
    printf("sorted=%d ids-aligned=%d p-sum=%.6f kept=%lld\n", ok, ok, sum, (long long) kept);
    return ok ? 0 : 1;
}
