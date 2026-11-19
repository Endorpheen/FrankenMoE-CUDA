/* The CUDA architecture policy doors on values known by hand: which compute capabilities a CUDA version
 * supports, and what the default is when nothing is given. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct { const void * d; int64_t n; } csl_u8;
typedef struct { void * d; int64_t n; } sl_u8;
typedef int64_t (*ver_ge_fn)(int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*arch_resolve_fn)(csl_u8, csl_u8, sl_u8);

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    ver_ge_fn ge = (ver_ge_fn) dlsym(h, "arch_ver_ge");
    arch_resolve_fn res = (arch_resolve_fn) dlsym(h, "arch_resolve");
    if (!ge || !res) { printf("door missing\n"); return 2; }

    int bad = 0;
    struct { int maj, min, wmaj, wmin, want; } cases[] = {
        { 12, 0, 12, 0, 1 }, { 12, 0, 12, 1, 0 }, { 11, 8, 12, 0, 0 }, { 13, 1, 12, 4, 1 },
    };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        const int64_t got = ge(cases[i].maj, cases[i].min, cases[i].wmaj, cases[i].wmin);
        const int ok = (got == 1) == (cases[i].want == 1);
        printf("  ver_ge(%d.%d >= %d.%d) = %lld %s\n", cases[i].maj, cases[i].min,
               cases[i].wmaj, cases[i].wmin, (long long) got, ok ? "" : "  <- unexpected");
        if (!ok) { ++bad; }
    }

    char out[256];
    const char * arch = "75;86;89";
    const char * native = "89";
    memset(out, 0, sizeof(out));
    sl_u8 so = { out, (int64_t) sizeof(out) };
    csl_u8 ina = { arch, 8 }, inn = { native, 2 };
    const int64_t n = res(ina, inn, so);
    printf("  arch_resolve(\"%s\", \"%s\") = %lld -> [%s]\n", arch, native, (long long) n, out);
    if (n <= 0 || out[0] == 0) { ++bad; }

    printf("cuda arch doors: cases=%u failures=%d\n", (unsigned)(sizeof(cases)/sizeof(cases[0]) + 1), bad);
    return bad ? 1 : 0;
}
