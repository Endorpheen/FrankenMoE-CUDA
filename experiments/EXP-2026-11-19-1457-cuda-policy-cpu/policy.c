// The CUDA version and flag decisions, asked directly on the CPU.
//
// Both doors are pure: they take numbers and a mode string and return text. The host that will use them is CMake, which
// cannot call them, so this is the only place their branch conditions can be pinned today - and a branch that changes
// silently changes what every build compiles.
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

struct sl_u8 { const void * data; int64_t len; };

typedef int64_t (*arch_default_fn)(int64_t, int64_t, int64_t, int64_t, int64_t, struct sl_u8);
typedef int64_t (*tgt_flags_fn)(int64_t, int64_t, int64_t, int64_t, struct sl_u8, struct sl_u8);

static struct sl_u8 u8s(const char * s) { struct sl_u8 r = { s, (int64_t) strlen(s) }; return r; }

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "builds/flint-acc/bin/libllama.so.0";
    void * h = dlopen(path, RTLD_LAZY);
    if (!h) { printf("cannot open %s: %s\n", path, dlerror()); return 2; }
    arch_default_fn def = (arch_default_fn) dlsym(h, "arch_default");
    tgt_flags_fn flags = (tgt_flags_fn) dlsym(h, "tgt_flags");
    if (!def || !flags) { printf("a door is missing\n"); return 2; }

    int fails = 0;
    char out[512];

    // arch_default: the four branch points the module distinguishes, as the source states them
    struct { int cm, cn, native, km, kn; const char * must_contain; const char * must_not; } cases[] = {
        // native is only used when CUDA is at least 11.6 and CMake at least 3.24
        { 12, 0, 1, 3, 24, "native", nullptr },
        { 11, 5, 1, 4, 0, "75-virtual", "native" },          // CUDA too old for native
        { 12, 0, 1, 3, 23, "75-virtual", "native" },          // CMake too old for native
        { 12, 0, 0, 4, 0, "86-real", nullptr },               // native not requested
        { 13, 0, 0, 4, 0, "120a-real", "50-virtual" },        // CUDA 13 drops the oldest
        { 12, 9, 0, 4, 0, "121a-real", nullptr },
        { 11, 7, 0, 4, 0, "86-real", "89-real" },             // 89 and 90 need 11.8
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        memset(out, 0, sizeof(out));
        const int64_t n = def(cases[i].cm, cases[i].cn, cases[i].native, cases[i].km, cases[i].kn,
                              (struct sl_u8){ out, (int64_t) sizeof(out) - 1 });
        const int ok_len = n > 0 && n == (int64_t) strlen(out);
        const int has  = strstr(out, cases[i].must_contain) != nullptr;
        const int noth = cases[i].must_not == nullptr || strstr(out, cases[i].must_not) == nullptr;
        if (!ok_len || !has || !noth) {
            ++fails;
            printf("  ARCH_DEFAULT %d.%d native=%d cmake=%d.%d -> %.120s\n", cases[i].cm, cases[i].cn,
                   cases[i].native, cases[i].km, cases[i].kn, out);
        }
    }

    // tgt_flags: -lineinfo only in debug, compress-mode only from 12.8, -Werror only when fatal
    struct { int m, n, debug, fatal; const char * mode; const char * has; const char * has_not; } fcases[] = {
        { 12, 0, 0, 0, "size", "-use_fast_math", "-lineinfo" },
        { 12, 0, 1, 0, "size", "-lineinfo", "-Werror" },
        { 12, 0, 0, 1, "size", "-Werror", "-lineinfo" },
        { 12, 8, 0, 0, "speed", "-compress-mode=speed", "-compress-mode=size" },
        { 12, 7, 0, 0, "size", "-extended-lambda", "-compress-mode=" },
        { 12, 0, 0, 0, "size", "-use_fast_math", "-G" },   // -G disables optimisation and must never appear
    };
    long fc = 0;
    for (size_t i = 0; i < sizeof(fcases) / sizeof(fcases[0]); ++i) {
        memset(out, 0, sizeof(out));
        const int64_t n = flags(fcases[i].m, fcases[i].n, fcases[i].debug, fcases[i].fatal, u8s(fcases[i].mode),
                                (struct sl_u8){ out, (int64_t) sizeof(out) - 1 });
        ++fc;
        const int ok_len = n > 0 && n == (int64_t) strlen(out);
        const int has  = strstr(out, fcases[i].has) != nullptr;
        const int noth = fcases[i].has_not == nullptr || strstr(out, fcases[i].has_not) == nullptr;
        if (!ok_len || !has || !noth) {
            ++fails;
            printf("  TGT_FLAGS %d.%d debug=%d fatal=%d mode=%s -> %.160s\n", fcases[i].m, fcases[i].n,
                   fcases[i].debug, fcases[i].fatal, fcases[i].mode, out);
        }
    }

    printf("cuda policy: arch_default_cases=%zu tgt_flags_cases=%ld failures=%d\n",
           sizeof(cases) / sizeof(cases[0]), fc, fails);
    dlclose(h);
    return fails == 0 ? 0 : 1;
}
