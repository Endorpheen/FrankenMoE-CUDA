/* fg_sp_compatible against the native rule written out from common_speculative_are_compatible:
 * equal type; equal add_bos and, when set, equal bos; equal add_eos and, when set, equal eos; and
 * |n_vocab_t - n_vocab_d| <= max_diff. The per-token text comparison needs vocab text and is not part of
 * the door, so it is not part of this comparison either. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>

typedef int64_t (*cmp_fn)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                          int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

static int native_ok(int64_t vt, int64_t ab, int64_t b, int64_t ae, int64_t e, int64_t nv,
                     int64_t vd, int64_t abd, int64_t bd, int64_t aed, int64_t ed, int64_t nvd,
                     int64_t max_diff) {
    if (vt != vd) return 0;
    if (ab != abd) return 0;
    if (ab != 0 && b != bd) return 0;
    if (ae != aed) return 0;
    if (ae != 0 && e != ed) return 0;
    int64_t d = nv - nvd; if (d < 0) d = -d;
    if (d > max_diff) return 0;
    return 1;
}

int main(int argc, char ** argv) {
    const char * lib = argc > 1 ? argv[1] : "libllama.so.0";
    void * h = dlopen(lib, RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    cmp_fn fn = (cmp_fn) dlsym(h, "fg_sp_compatible");
    if (!fn) { printf("symbol missing\n"); return 2; }

    /* each case flips exactly one field, so a wrong field is identifiable */
    const int64_t base[12] = { 2, 1, 1, 1, 2, 32000, 2, 1, 1, 1, 2, 32000 };
    const int64_t maxd[3] = { 0, 128, 1024 };
    long calls = 0, bad = 0;
    for (int field = 0; field < 12; ++field) {
        for (int delta = -2; delta <= 2; ++delta) {
            for (int mi = 0; mi < 3; ++mi) {
                int64_t v[12]; for (int i = 0; i < 12; ++i) v[i] = base[i];
                v[field] += delta;
                const int64_t g = fn(v[0],v[1],v[2],v[3],v[4],v[5], v[6],v[7],v[8],v[9],v[10],v[11], maxd[mi]);
                const int n = native_ok(v[0],v[1],v[2],v[3],v[4],v[5], v[6],v[7],v[8],v[9],v[10],v[11], maxd[mi]);
                ++calls;
                if ((g == 1) != n) {
                    ++bad;
                    if (bad <= 8) printf("DIFF field=%d delta=%d max_diff=%lld goose=%lld native=%d\n",
                                          field, delta, (long long) maxd[mi], (long long) g, n);
                }
            }
        }
    }
    printf("compatibility door: calls=%ld disagreements=%ld\n", calls, bad);
    return bad ? 1 : 0;
}
