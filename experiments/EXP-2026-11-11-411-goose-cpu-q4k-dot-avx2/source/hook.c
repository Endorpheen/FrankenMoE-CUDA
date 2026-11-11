// Runtime replacement for the dispatched q4_K x q8_K dot.
//
// LD_PRELOAD cannot redirect this call: libggml-cpu binds its own symbol internally. So this constructor
// finds the symbol inside the already-loaded libggml-cpu, saves its first bytes and overwrites them with an
// absolute jump to the Goose kernel. FG411S_ENABLED=0 restores the original bytes instead of patching.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

extern float fg410s_dot(int64_t nb, const void * q4, const void * q8);

typedef void (*dot_fn)(int, float *, size_t, const void *, size_t, const void *, size_t, int);

static dot_fn g_native;
static unsigned char g_saved[12];
static void * g_target;
static unsigned long g_calls;
static int g_patched;

static void fg411s_entry(int n, float * s, size_t bs, const void * vx, size_t bx,
                         const void * vy, size_t by, int nrc) {
    const int64_t nb = (int64_t) (n / 256);
    g_calls++;
    if (getenv("FG411S_WITNESS") && g_calls == 1)
        fprintf(stderr, "FG411S_WITNESS calls=%lu nb=%lld sym=patched\n", g_calls, (long long) nb);
    *s = fg410s_dot(nb, (const char *) vx + bx * nrc, (const char *) vy + by * nrc);
}

static int fg411s_on(void) {
    const char * e = getenv("FG411S_ENABLED");
    return !(e && e[0] == '0');
}

__attribute__((constructor)) static void fg411s_init(void) {
    void * h = dlopen("libggml-cpu.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!h) return;
    g_target = dlsym(h, "ggml_vec_dot_q4_K_q8_K");
    g_native = (dot_fn) g_target;
    if (!g_target || !fg411s_on()) {
        if (getenv("FG411S_WITNESS"))
            fprintf(stderr, "FG411S_PATCH skipped target=%p enabled=%d\n", g_target, fg411s_on());
        return;
    }

    long page = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t) (page - 1));
    if (mprotect(base, (size_t) page, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return;
    memcpy(g_saved, g_target, sizeof(g_saved));
    unsigned char patch[12] = { 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0 };  // movabs rax, imm64; jmp rax
    uintptr_t dest = (uintptr_t) &fg411s_entry;
    memcpy(&patch[2], &dest, 8);
    memcpy(g_target, patch, sizeof(patch));
    __builtin___clear_cache((char *) g_target, (char *) g_target + sizeof(patch));
    g_patched = 1;
    if (getenv("FG411S_WITNESS"))
        fprintf(stderr, "FG411S_PATCH target=%p entry=%p enabled\n", g_target, (void *) &fg411s_entry);
}

__attribute__((destructor)) static void fg411s_fini(void) {
    if (!g_patched || !g_target) return;
    long page = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t) (page - 1));
    if (mprotect(base, (size_t) page, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        memcpy(g_target, g_saved, sizeof(g_saved));
        __builtin___clear_cache((char *) g_target, (char *) g_target + sizeof(g_saved));
    }
    fprintf(stderr, "FG411S_PATCH restored calls=%lu\n", g_calls);
}
