// Entry counter for ggml_vec_dot_q4_K_q8_K in the accepted CPU backend.
// Delegates to the accepted EXP665 row object (bit-exact with the dispatched
// AVX2 vec_dot) so the function keeps working while being counted.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct { const uint8_t *data; int64_t len; } fg_cpu_simd_bytes;
extern int32_t fg_cpu_simd_row_q4_k_q8_k(fg_cpu_simd_bytes a, fg_cpu_simd_bytes b, int32_t nb, float * value);

static void * g_target; static unsigned char g_saved[12]; static unsigned long g_calls; static int g_on;

static void shim(int n, float * s, size_t bs, const void * vx, size_t bx,
                 const void * vy, size_t by, int nrc) {
    (void) bs;
    g_calls++;
    if (g_calls == 1) fprintf(stderr, "VECDOT_FIRST n=%d nrc=%d bx=%zu by=%zu\n", n, nrc, bx, by);
    if (nrc != 1 || (n % 256) != 0) {
        fprintf(stderr, "VECDOT_UNEXPECTED n=%d nrc=%d\n", n, nrc);
        _exit(99);
    }
    const int32_t nb = n / 256;
    const int32_t rc = fg_cpu_simd_row_q4_k_q8_k(
        (fg_cpu_simd_bytes){ (const uint8_t *) vx, (int64_t) nb * 144 },
        (fg_cpu_simd_bytes){ (const uint8_t *) vy, (int64_t) nb * 292 }, nb, s);
    if (rc != 0) { fprintf(stderr, "VECDOT_GOOSE_RC %d\n", rc); _exit(98); }
}

__attribute__((constructor)) static void init(void) {
    if (getenv("FG_VECDOT_OFF") && !strcmp(getenv("FG_VECDOT_OFF"), "1")) return;
    void * h = dlopen("libggml-cpu.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!h) { fprintf(stderr, "VECDOT_NO_LIB\n"); _exit(97); }
    g_target = dlsym(h, "ggml_vec_dot_q4_K_q8_K");
    if (!g_target) { fprintf(stderr, "VECDOT_NO_SYMBOL\n"); _exit(96); }
    long page = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t)(page-1));
    if (mprotect(base, (size_t) page, PROT_READ|PROT_WRITE|PROT_EXEC) != 0) _exit(95);
    memcpy(g_saved, g_target, sizeof(g_saved));
    unsigned char patch[12] = {0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE0};
    uintptr_t dest = (uintptr_t) &shim; memcpy(&patch[2], &dest, 8);
    memcpy(g_target, patch, sizeof(patch));
    __builtin___clear_cache((char*)g_target, (char*)g_target + sizeof(patch));
    g_on = 1;
    fprintf(stderr, "VECDOT_PATCHED %p\n", g_target);
}
__attribute__((destructor)) static void fini(void) {
    if (g_on && g_target) {
        long page = sysconf(_SC_PAGESIZE);
        void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t)(page-1));
        if (mprotect(base, (size_t) page, PROT_READ|PROT_WRITE|PROT_EXEC) == 0)
            memcpy(g_target, g_saved, sizeof(g_saved));
    }
    fprintf(stderr, "VECDOT_TOTAL %lu\n", g_calls);
}
