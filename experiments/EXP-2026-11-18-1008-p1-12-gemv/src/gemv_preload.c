// A/B hook: redirect the live Q4_K gemv to the Goose Ironbeak object.
// The reference binary is not modified; the symbol entry is patched in memory
// and restored by the destructor. FG_IRONBEAK_HOOK=0 disables the patch.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct { const uint8_t *data; int64_t len; } fg_cpu_simd_bytes;
extern int32_t fg_cpu_simd_row_q4_k_q8_k_x8(fg_cpu_simd_bytes w, fg_cpu_simd_bytes a,
                                            int32_t nb, int32_t nc, float * out);
typedef void (*gemv_fn)(int, float *, size_t, const void *, const void *, int, int);

static void * g_target;
static unsigned char g_saved[12];
static unsigned long g_calls;
static int g_patched;

static void fg_ironbeak_entry(int n, float * s, size_t bs, const void * vx,
                              const void * vy, int nr, int nc) {
    (void) bs;
    g_calls++;
    if (g_calls == 1 && getenv("FG_IRONBEAK_WITNESS")) {
        fprintf(stderr, "IRONBEAK_HOOK first_call n=%d nc=%d nr=%d\n", n, nc, nr);
    }
    if (nr != 1 || (n % 256) != 0 || (nc % 8) != 0) {
        fprintf(stderr, "IRONBEAK_HOOK unexpected call n=%d nc=%d nr=%d\n", n, nc, nr);
        _exit(99);
    }
    const int64_t nb = n / 256;
    const int32_t rc = fg_cpu_simd_row_q4_k_q8_k_x8(
        (fg_cpu_simd_bytes){ (const uint8_t *) vx, (int64_t) nc * nb * 144 },
        (fg_cpu_simd_bytes){ (const uint8_t *) vy, nb * 292 },
        (int32_t) nb, nc, s);
    if (rc != 0) {
        fprintf(stderr, "IRONBEAK_HOOK goose rc=%d n=%d nc=%d nb=%lld\n", rc, n, nc, (long long) nb);
        _exit(98);
    }
}

__attribute__((constructor)) static void fg_ironbeak_init(void) {
    const char * e = getenv("FG_IRONBEAK_HOOK");
    if (e && e[0] == '0') {
        if (getenv("FG_IRONBEAK_WITNESS")) fprintf(stderr, "IRONBEAK_HOOK disabled\n");
        return;
    }
    void * h = dlopen("libggml-cpu.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!h) { fprintf(stderr, "IRONBEAK_HOOK no libggml-cpu\n"); _exit(97); }
    g_target = dlsym(h, "ggml_gemv_q4_K_8x8_q8_K");
    if (!g_target) { fprintf(stderr, "IRONBEAK_HOOK no symbol\n"); _exit(96); }

    long page = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t) (page - 1));
    if (mprotect(base, (size_t) page, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) _exit(95);
    memcpy(g_saved, g_target, sizeof(g_saved));
    unsigned char patch[12] = { 0x48, 0xB8, 0,0,0,0,0,0,0,0, 0xFF, 0xE0 };
    uintptr_t dest = (uintptr_t) &fg_ironbeak_entry;
    memcpy(&patch[2], &dest, 8);
    memcpy(g_target, patch, sizeof(patch));
    __builtin___clear_cache((char *) g_target, (char *) g_target + sizeof(patch));
    g_patched = 1;
    if (getenv("FG_IRONBEAK_WITNESS")) fprintf(stderr, "IRONBEAK_HOOK patched %p\n", g_target);
}

__attribute__((destructor)) static void fg_ironbeak_fini(void) {
    if (!g_patched) return;
    long page = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t) (page - 1));
    if (mprotect(base, (size_t) page, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        memcpy(g_target, g_saved, sizeof(g_saved));
        __builtin___clear_cache((char *) g_target, (char *) g_target + sizeof(g_saved));
    }
    fprintf(stderr, "IRONBEAK_HOOK restored calls=%lu\n", g_calls);
}
