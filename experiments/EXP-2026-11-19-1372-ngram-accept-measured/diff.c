/* fg_sp_ngram_accept against the native rule written out from the ngram-modulation accept() path:
 *   if n_draft_last > 0:
 *       f_acc = n_accepted / n_draft_last
 *       if f_acc < 0.25: n_low++; if n_low >= 5: reset, n_low = 0
 *       else: n_low = 0
 * The door answers (n_low', reset) so the host can apply it; the comparison is on those two numbers.
 * The < 0.25 test is written as 4*n_accepted < n_draft_last in the port, which is exact in integers where
 * the division is not - the grid includes the boundary where that could matter. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>

typedef struct { void * data; int64_t len; } sl_i64;
typedef int64_t (*ngram_fn)(int64_t, int64_t, int64_t, sl_i64);

int main(int argc, char ** argv) {
    const char * lib = argc > 1 ? argv[1] : "libllama.so.0";
    void * h = dlopen(lib, RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    ngram_fn fn = (ngram_fn) dlsym(h, "fg_sp_ngram_accept");
    if (!fn) { printf("symbol missing\n"); return 2; }

    const int64_t acc[]   = { -1, 0, 1, 2, 3, 4, 5, 25, 100 };
    const int64_t draft[] = { -1, 0, 1, 2, 3, 4, 5, 8, 16, 100 };
    const int64_t low[]   = { 0, 1, 2, 3, 4, 5, 9 };

    long calls = 0, bad = 0;
    for (unsigned a = 0; a < sizeof(acc)/sizeof(acc[0]); ++a)
    for (unsigned d = 0; d < sizeof(draft)/sizeof(draft[0]); ++d)
    for (unsigned l = 0; l < sizeof(low)/sizeof(low[0]); ++l) {
        int64_t out[2] = { -7, -7 };
        sl_i64 s = { out, 2 };
        const int64_t r = fn(acc[a], draft[d], low[l], s);

        int64_t n_low_want = low[l], reset_want = 0;
        if (draft[d] > 0) {
            if ((double) acc[a] / (double) draft[d] < 0.25) {
                n_low_want++;
                if (n_low_want >= 5) { n_low_want = 0; reset_want = 1; }
            } else {
                n_low_want = 0;
            }
        }
        ++calls;
        if (r != 0 || out[0] != n_low_want || out[1] != reset_want) {
            ++bad;
            if (bad <= 8) printf("DIFF acc=%lld draft=%lld low=%lld -> r=%lld out=(%lld,%lld) want=(%lld,%lld)\n",
                                 (long long) acc[a], (long long) draft[d], (long long) low[l], (long long) r,
                                 (long long) out[0], (long long) out[1],
                                 (long long) n_low_want, (long long) reset_want);
        }
    }
    printf("ngram-accept door: calls=%ld disagreements=%ld\n", calls, bad);
    return bad ? 1 : 0;
}
