// Count-only entry redirect for ggml_vec_dot_q4_K_q8_K.
// The first 13 bytes of the function end on an instruction boundary
// (endbr64, push rbp, mov rsi,r10, mov rsp,rbp, push r14), so a trampoline can
// replay them and jump back. Arithmetic is unchanged: this arm is native.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

__attribute__((visibility("hidden"))) unsigned long vecdot_count;
__attribute__((visibility("hidden"))) void * vecdot_tramp;
extern void vecdot_count_stub(void);

static void * g_target; static unsigned char g_saved[13];

__attribute__((constructor)) static void init(void) {
    void * h = dlopen("libggml-cpu.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!h) { fprintf(stderr, "VECDOT_COUNT_NOLIB\n"); _exit(97); }
    g_target = dlsym(h, "ggml_vec_dot_q4_K_q8_K");
    if (!g_target) { fprintf(stderr, "VECDOT_COUNT_NOSYM\n"); _exit(96); }

    unsigned char * tramp = mmap(NULL, 4096, PROT_READ|PROT_WRITE|PROT_EXEC,
                                 MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (tramp == MAP_FAILED) _exit(95);
    memcpy(tramp, g_target, 13);
    tramp[13] = 0x48; tramp[14] = 0xB8;             // movabs rax, target+13
    uintptr_t back = (uintptr_t) g_target + 13;
    memcpy(&tramp[15], &back, 8);
    tramp[23] = 0xFF; tramp[24] = 0xE0;             // jmp rax
    vecdot_tramp = tramp;

    long page = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t)(page-1));
    if (mprotect(base, (size_t) page, PROT_READ|PROT_WRITE|PROT_EXEC) != 0) _exit(94);
    memcpy(g_saved, g_target, 13);
    unsigned char patch[13] = { 0x48, 0xB8, 0,0,0,0,0,0,0,0, 0xFF, 0xE0, 0x90 };
    uintptr_t dest = (uintptr_t) &vecdot_count_stub;
    memcpy(&patch[2], &dest, 8);
    memcpy(g_target, patch, 13);
    __builtin___clear_cache((char*)g_target, (char*)g_target + 13);
    fprintf(stderr, "VECDOT_COUNT_PATCHED %p\n", g_target);
}
__attribute__((destructor)) static void fini(void) {
    if (g_target) {
        long page = sysconf(_SC_PAGESIZE);
        void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t)(page-1));
        if (mprotect(base, (size_t) page, PROT_READ|PROT_WRITE|PROT_EXEC) == 0)
            memcpy(g_target, g_saved, 13);
    }
    fprintf(stderr, "VECDOT_COUNT_TOTAL %lu\n", vecdot_count);
}
