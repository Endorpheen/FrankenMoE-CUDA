// Shim for the Goose Q6_K CUDA matvec module (EXP186).
//
// The module compiles unchanged against the flint tree and exports ggml_cuda_mul_mat_vec_q itself. CUDA calls
// from inside libggml-cuda are bound internally, so preloading alone cannot redirect them: this shim patches the
// entry of the original symbol and hands the module a way back to it under the name it expects
// (fg185q_native_ggml_cuda_mul_mat_vec_q). GGML_GOOSE_CUDA_MMVQ_Q6K=0 disables the patch.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef void (*mmvq_fn)(void *, void *, void *, void *, void *, void *);

static mmvq_fn g_native;
static unsigned char g_saved[16];
static void * g_target;
static int g_patched;

/* the module calls the native kernel under this name */
void fg185q_native_ggml_cuda_mul_mat_vec_q(void * a, void * b, void * c, void * d, void * e, void * f) {
    if (!g_native) return;
    g_native(a, b, c, d, e, f);
}

static int enabled(void) {
    const char * e = getenv("GGML_GOOSE_CUDA_MMVQ_Q6K");
    return !(e && strcmp(e, "0") == 0);
}

__attribute__((constructor)) static void init(void) {
    if (!enabled()) return;
    void * h = dlopen("libggml-cuda.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!h) return;
    /* mangled ggml_cuda_mul_mat_vec_q(ggml_backend_cuda_context&, ggml_tensor const*, ...) */
    g_target = dlsym(h, "_Z23ggml_cuda_mul_mat_vec_qR25ggml_backend_cuda_contextPK11ggml_tensorS3_S3_PS1_PK29ggml_cuda_mm_fusion_args_host");
    if (!g_target) return;
    g_native = (mmvq_fn) g_target;

    /* our replacement lives in this library and has the same mangled name */
    void * ours = dlsym(RTLD_DEFAULT, "_Z23ggml_cuda_mul_mat_vec_qR25ggml_backend_cuda_contextPK11ggml_tensorS3_S3_PS1_PK29ggml_cuda_mm_fusion_args_host");
    if (!ours || ours == g_target) {
        if (getenv("FG186S_WITNESS")) fprintf(stderr, "FG186S_PATCH skipped ours=%p target=%p\n", ours, g_target);
        return;
    }
    long page = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t) (page - 1));
    if (mprotect(base, (size_t) page, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return;
    memcpy(g_saved, g_target, sizeof(g_saved));
    unsigned char patch[12] = { 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0 };
    uintptr_t dest = (uintptr_t) ours;
    memcpy(&patch[2], &dest, 8);
    memcpy(g_target, patch, sizeof(patch));
    __builtin___clear_cache((char *) g_target, (char *) g_target + sizeof(patch));
    g_patched = 1;
    if (getenv("FG186S_WITNESS"))
        fprintf(stderr, "FG186S_PATCH target=%p ours=%p enabled\n", g_target, ours);
}

__attribute__((destructor)) static void fini(void) {
    if (!g_patched || !g_target) return;
    long page = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t) (page - 1));
    if (mprotect(base, (size_t) page, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        memcpy(g_target, g_saved, sizeof(g_saved));
        __builtin___clear_cache((char *) g_target, (char *) g_target + (int) sizeof(g_saved));
    }
    if (getenv("FG186S_WITNESS")) fprintf(stderr, "FG186S_PATCH restored\n");
}
