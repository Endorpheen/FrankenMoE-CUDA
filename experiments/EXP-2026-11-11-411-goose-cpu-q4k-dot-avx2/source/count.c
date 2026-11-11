// Counting detours for the CPU dot kernels, to find which ones the profile actually calls.
//
// LD_PRELOAD cannot redirect internally bound calls, so each target's entry is overwritten with an absolute
// jump to a small wrapper. The wrapper calls the original through a trampoline (the saved bytes plus a jump
// back) and counts calls per kernel. FG411S_ENABLED=0 disables patching.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <link.h>

typedef void (*dot_fn)(int, float *, size_t, const void *, size_t, const void *, size_t, int);

#define NAMES(X) \
    X(q4_K_q8_K) X(q6_K_q8_K) X(q8_0_q8_0) X(q2_K_q8_K) X(q3_K_q8_K) X(q5_K_q8_K)

#define DECL(n) static dot_fn nat_##n; static void * tgt_##n; static unsigned long cnt_##n; \
               static unsigned char sav_##n[16]; static int det_##n;
NAMES(DECL)

static long page_size;
static int enabled_flag = 1;

static void * make_trampoline(void * target) {
    unsigned char * t = mmap(NULL, 64, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (t == MAP_FAILED) return NULL;
    memcpy(t, target, 12);
    t[12] = 0x48; t[13] = 0xB8;
    uintptr_t back = (uintptr_t) target + 12;
    memcpy(&t[14], &back, 8);
    t[22] = 0xFF; t[23] = 0xE0;
    return t;
}

static int install(void * target, void (*wrapper)(void), unsigned char * saved) {
    if (!target) return 0;
    void * base = (void *) ((uintptr_t) target & ~(uintptr_t) (page_size - 1));
    if (mprotect(base, (size_t) page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return 0;
    memcpy(saved, target, 16);
    unsigned char patch[12] = { 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0 };
    uintptr_t dest = (uintptr_t) wrapper;
    memcpy(&patch[2], &dest, 8);
    memcpy(target, patch, sizeof(patch));
    __builtin___clear_cache((char *) target, (char *) target + sizeof(patch));
    return 1;
}

#define WRAP(n) static void wrap_##n(int a, float * s, size_t bs, const void * vx, size_t bx, \
                                     const void * vy, size_t by, int nrc) { \
    cnt_##n++; \
    ((dot_fn) tramp_##n)(a, s, bs, vx, bx, vy, by, nrc); }
static void * tramp_q4_K_q8_K; static void * tramp_q6_K_q8_K; static void * tramp_q8_0_q8_0;
static void * tramp_q2_K_q8_K; static void * tramp_q3_K_q8_K; static void * tramp_q5_K_q8_K;
NAMES(WRAP)

static void patch_object(const char * path) {
    void * c = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
    if (!c) return;
    void * t = dlsym(c, "ggml_vec_dot_q4_K_q8_K");
    if (!t) { dlclose(c); return; }
    if (getenv("FG411S_WITNESS")) fprintf(stderr, "FG411S_LIB %s %p\n", path, t);
#define SETUP(n) do { \
        tgt_##n = dlsym(c, "ggml_vec_dot_" #n); \
        nat_##n = (dot_fn) tgt_##n; \
        tramp_##n = make_trampoline(tgt_##n); \
        if (tramp_##n) install(tgt_##n, (void (*)(void)) wrap_##n, sav_##n); \
    } while (0);
    NAMES(SETUP)
}

static int cb(struct dl_phdr_info * info, size_t size, void * data) {
    (void) size; (void) data;
    if (info->dlpi_name && strstr(info->dlpi_name, "ggml-cpu")) patch_object(info->dlpi_name);
    return 0;
}

__attribute__((constructor)) static void init2(void) {
    page_size = sysconf(_SC_PAGESIZE);
    const char * e = getenv("FG411S_ENABLED");
    enabled_flag = !(e && e[0] == '0');
    if (!enabled_flag) return;
    dl_iterate_phdr(cb, NULL);
    if (getenv("FG411S_WITNESS")) fprintf(stderr, "FG411S_DETOURS done\n");
}

__attribute__((unused)) static void init_old(void) {
    page_size = sysconf(_SC_PAGESIZE);
    const char * e = getenv("FG411S_ENABLED");
    enabled_flag = !(e && e[0] == '0');
    void * h = NULL;
    {
        const char * candidates[] = {
            "/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/stage-run/libggml-cpu-shared-dd5a40e47065.so.0.27.0",
            "/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/stage-run/libggml-cpu.so.0",
            "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin/libggml-cpu.so.0",
        };
        for (unsigned i = 0; i < sizeof(candidates)/sizeof(candidates[0]) && !h; ++i) {
            void * c = dlopen(candidates[i], RTLD_NOW | RTLD_NOLOAD);
            if (!c) continue;
            if (dlsym(c, "ggml_vec_dot_q4_K_q8_K")) { h = c; if (getenv("FG411S_WITNESS")) fprintf(stderr, "FG411S_LIB %s\n", candidates[i]); }
            else dlclose(c);
        }
    }
    if (!h || !enabled_flag) return;
#define SETUP(n) do { \
        tgt_##n = dlsym(h, "ggml_vec_dot_" #n); \
        nat_##n = (dot_fn) tgt_##n; \
        tramp_##n = make_trampoline(tgt_##n); \
        det_##n = tramp_##n ? install(tgt_##n, (void (*)(void)) wrap_##n, sav_##n) : 0; \
    } while (0);
    NAMES(SETUP)
    if (getenv("FG411S_WITNESS"))
        fprintf(stderr, "FG411S_DETOURS q4_K=%d q6_K=%d q8_0=%d q2_K=%d q3_K=%d q5_K=%d\n",
                det_q4_K_q8_K, det_q6_K_q8_K, det_q8_0_q8_0, det_q2_K_q8_K, det_q3_K_q8_K, det_q5_K_q8_K);
}

__attribute__((destructor)) static void fini(void) {
    fprintf(stderr, "FG411S_COUNT q4_K=%lu q6_K=%lu q8_0=%lu q2_K=%lu q3_K=%lu q5_K=%lu\n",
            cnt_q4_K_q8_K, cnt_q6_K_q8_K, cnt_q8_0_q8_0, cnt_q2_K_q8_K, cnt_q3_K_q8_K, cnt_q5_K_q8_K);
}
