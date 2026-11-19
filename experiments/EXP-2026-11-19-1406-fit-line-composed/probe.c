/* fp_line on a known value: the door is composed into the unit now, and a door is not "available" because it
 * links - it is available when it answers a case whose answer is known by hand. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct { void * d; int64_t n; } sl_i64;
typedef struct { void * d; int64_t n; } sl_u8;
typedef struct { void * d; int64_t n; } sl_f64;
typedef int64_t (*fp_line_fn)(int64_t, int64_t, sl_f64, int64_t, sl_u8, sl_i64, sl_i64,
                              sl_u8, sl_i64, sl_i64, int64_t, sl_u8);

int main(int argc, char ** argv) {
    const char * lib = argc > 1 ? argv[1] : "libllama.so.0";
    void * h = dlopen(lib, RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    fp_line_fn fp = (fp_line_fn) dlsym(h, "fp_line");
    if (!fp) { printf("fp_line missing\n"); return 2; }

    char out[512]; memset(out, 0, sizeof(out));
    const double splits[1] = { 1.0 };
    char pats[8]; int64_t poff[4], plen[4]; char bufts[8]; int64_t boff[4], blen[4];
    sl_f64 s_splits = { (void *) splits, 1 };
    sl_u8  s_pats   = { pats, 0 };
    sl_i64 s_poff   = { poff, 0 };
    sl_i64 s_plen   = { plen, 0 };
    sl_u8  s_bufts  = { bufts, 0 };
    sl_i64 s_boff   = { boff, 0 };
    sl_i64 s_blen   = { blen, 0 };
    sl_u8  s_out    = { out, (int64_t) sizeof(out) };
    const int64_t n = fp(4096, 41, s_splits, 1, s_pats, s_poff, s_plen, s_bufts, s_boff, s_blen, 0, s_out);
    printf("n=%lld line=[%s]\n", (long long) n, out);
    return 0;
}
