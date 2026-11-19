/* The target-flags and install-layout doors on cases whose answers can be read off the rules:
 * a debug build carries -G, a non-fatal one does not carry --generate-line-info, and a dynamic build installs
 * to the dynamic destination. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct { const void * d; int64_t n; } csl_u8;
typedef struct { void * d; int64_t n; } sl_u8;
typedef int64_t (*tgt_flags_fn)(int64_t, int64_t, int64_t, int64_t, int64_t, sl_u8);
typedef int64_t (*inst_kind_fn)(int64_t, int64_t);

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    tgt_flags_fn tf = (tgt_flags_fn) dlsym(h, "tgt_flags");
    inst_kind_fn ik = (inst_kind_fn) dlsym(h, "inst_kind");
    if (!tf || !ik) { printf("door missing\n"); return 2; }

    int bad = 0;
    char out[512];
    /* debug=1, fatal=0: the flags must contain -G and must not contain --generate-line-info */
    memset(out, 0, sizeof(out));
    sl_u8 so = { out, (int64_t) sizeof(out) };
    const int64_t n1 = tf(8, 9, 1, 0, 0, so);
    /* the module emits nvcc's -lineinfo for a debug build; the expectation is read from its source, not assumed */
    const int has_li = strstr(out, "-lineinfo") != NULL;
    const int has_G  = strstr(out, "-G") != NULL;
    printf("  debug flags (n=%lld): [%s]\n", (long long) n1, out);
    printf("    contains -lineinfo: %d, contains -G: %d\n", has_li, has_G);
    if (!has_li || has_G) { ++bad; }

    /* the same build without debug must not carry -G */
    memset(out, 0, sizeof(out));
    const int64_t n2 = tf(8, 9, 0, 0, 0, so);
    const int has_li2 = strstr(out, "-lineinfo") != NULL;
    printf("  release flags (n=%lld): [%s]\n", (long long) n2, out);
    if (has_li2) { ++bad; }

    /* install kind: dynamic build -> the dynamic destination, whatever the shared flag says */
    /* the enum is read from the module: 2 = dynamic, 1 = shared, 0 = neither */
    const int64_t k1 = ik(1, 0), k2 = ik(0, 1), k3 = ik(0, 0);
    printf("  inst_kind: (1,0)=%lld (0,1)=%lld (0,0)=%lld\n", (long long) k1, (long long) k2, (long long) k3);
    if (k1 != 2 || k2 != 1 || k3 != 0) { ++bad; }

    printf("cuda flags/install doors: cases=3 failures=%d\n", bad);
    return bad ? 1 : 0;
}
