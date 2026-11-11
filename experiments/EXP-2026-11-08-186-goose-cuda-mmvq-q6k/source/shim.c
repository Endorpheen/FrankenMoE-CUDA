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
#include <signal.h>
#include <execinfo.h>

typedef void (*mmvq_fn)(void *, void *, void *, void *, void *, void *);

static mmvq_fn g_native;
static long page_size_;

/* a callable copy of the original entry, so the module's "native" call cannot land in our own patch */
static mmvq_fn make_trampoline(void * target) {
    unsigned char * t = mmap(NULL, 64, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (t == MAP_FAILED) return NULL;
    memcpy(t, target, 12);
    t[12] = 0x48; t[13] = 0xB8;
    uintptr_t back = (uintptr_t) target + 12;
    memcpy(&t[14], &back, 8);
    t[22] = 0xFF; t[23] = 0xE0;
    return (mmvq_fn) t;
}
static unsigned char g_saved[16];
static void * g_target;
static int g_patched;

/* the module calls the native kernel under its own C++ mangled name */
void fg185q_native_shim(void * a, void * b, void * c, void * d, void * e, void * f)
    __asm__("_Z37fg185q_native_ggml_cuda_mul_mat_vec_qR25ggml_backend_cuda_contextPK11ggml_tensorS3_S3_PS1_PK29ggml_cuda_mm_fusion_args_host");
void fg185q_native_shim(void * a, void * b, void * c, void * d, void * e, void * f) {
    if (!g_native) return;
    g_native(a, b, c, d, e, f);
}

static int enabled(void) {
    const char * e = getenv("GGML_GOOSE_CUDA_MMVQ_Q6K");
    return !(e && strcmp(e, "0") == 0);
}

static void * g_guard_page;
static volatile int g_guard_hits;
static void segv_probe(int sig, siginfo_t * si, void * uc) {
    (void) sig; (void) uc;
    if (si && si->si_addr == g_guard_page && g_guard_hits < 1) {
        g_guard_hits++;
        void * bt[24];
        int n = backtrace(bt, 24);
        fprintf(stderr, "FG186S_WRITER addr=%p frames=%d\n", si->si_addr, n);
        backtrace_symbols_fd(bt, n, 2);
        mprotect(g_guard_page, (size_t) sysconf(_SC_PAGESIZE), PROT_READ | PROT_WRITE);
        return;
    }
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
}

static void fg186_install_cinit(void * h);
typedef void * (*cidx_fn)(size_t);
static cidx_fn g_cinit_orig;
static void * cinit_wrap(size_t n) {
    void * be = g_cinit_orig ? g_cinit_orig(n) : NULL;
    if (be && getenv("FG186S_WITNESS")) {
        void * c = *(void **) ((char *) be + 0x90);
        fprintf(stderr, "FG186S_CINIT backend=%p ctx=%p slot0=%p\n", be, c, c ? *(void **) ((char *) c + 0xcf0) : NULL);
    }
    return be;
}

typedef void * (*dev_init_fn)(void *, const char *);
static dev_init_fn g_dev_orig;
void * ggml_backend_dev_init(void * dev, const char * params) {
    if (!g_dev_orig) {
        void * h = dlopen("libggml-base.so.0", RTLD_NOW | RTLD_NOLOAD);
        if (h) g_dev_orig = (dev_init_fn) dlsym(h, "ggml_backend_dev_init");
    }
    void * be = g_dev_orig ? g_dev_orig(dev, params) : NULL;
    if (be && getenv("FG186S_WITNESS")) {
        void * c = *(void **) ((char *) be + 0x90);
        fprintf(stderr, "FG186S_DEVINIT backend=%p ctx=%p slot0=%p\n", be, c, c ? *(void **) ((char *) c + 0xcf0) : NULL);
    }
    return be;
}

typedef void * (*init_fn)(size_t);
static init_fn g_init_orig;
void * ggml_backend_cuda_init(size_t n) {
    void * h = dlopen("libggml-cuda.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!g_init_orig && h) g_init_orig = (init_fn) dlsym(h, "ggml_backend_cuda_init");
    void * be = g_init_orig ? g_init_orig(n) : NULL;
    if (be) {
        long pg = sysconf(_SC_PAGESIZE);
        void ** ctxslot = (void **) ((char *) be + 0x90);
        void * c = *ctxslot;
        if (c) {
            void * slot = (void *) ((char *) c + 0xcf0);
            fprintf(stderr, "FG186S_ATINIT ctx=%p slot0=%p\n", c, *(void **) slot);
            g_guard_page = (void *) ((uintptr_t) slot & ~(uintptr_t) (pg - 1));
            struct sigaction sa; memset(&sa, 0, sizeof sa);
            sa.sa_sigaction = segv_probe; sa.sa_flags = SA_SIGINFO;
            sigaction(SIGSEGV, &sa, NULL);
            if (getenv("FG186S_GUARD") && mprotect(g_guard_page, (size_t) pg, PROT_NONE) == 0 && getenv("FG186S_WITNESS"))
                fprintf(stderr, "FG186S_GUARD armed at %p (slot %p)\n", g_guard_page, slot);
        }
    }
    if (be && getenv("FG186S_WITNESS")) {
        void ** ctxp = (void **) ((char *) be + 8);   /* ggml_backend.context field; printed for reference */
        fprintf(stderr, "FG186S_INIT backend=%p ctxguess=%p\n", be, ctxp ? *ctxp : NULL);
    }
    return be;
}

__attribute__((constructor)) static void init(void) {
    if (!enabled()) return;
    void * h = dlopen("libggml-cuda.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!h) return;
    /* mangled ggml_cuda_mul_mat_vec_q(ggml_backend_cuda_context&, ggml_tensor const*, ...) */
    g_target = dlsym(h, "_Z23ggml_cuda_mul_mat_vec_qR25ggml_backend_cuda_contextPK11ggml_tensorS3_S3_PS1_PK29ggml_cuda_mm_fusion_args_host");
    if (!g_target) return;
    g_native = make_trampoline(g_target);
    page_size_ = sysconf(_SC_PAGESIZE);

    /* our replacement lives in this library and has the same mangled name */
    void * ours = dlsym(RTLD_DEFAULT, "_Z23ggml_cuda_mul_mat_vec_qR25ggml_backend_cuda_contextPK11ggml_tensorS3_S3_PS1_PK29ggml_cuda_mm_fusion_args_host");
    if (!ours || ours == g_target) {
        if (getenv("FG186S_WITNESS")) fprintf(stderr, "FG186S_PATCH skipped ours=%p target=%p\n", ours, g_target);
        return;
    }
    long page = page_size_ ? page_size_ : sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t) (page - 1));
    if (mprotect(base, (size_t) page, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return;
    memcpy(g_saved, g_target, sizeof(g_saved));
    unsigned char patch[12] = { 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0 };
    uintptr_t dest = (uintptr_t) ours;
    memcpy(&patch[2], &dest, 8);
    memcpy(g_target, patch, sizeof(patch));
    __builtin___clear_cache((char *) g_target, (char *) g_target + sizeof(patch));
    g_patched = 1;
    {
        /* also detour the context-creation entry itself, so internal callers are covered */
        fg186_install_cinit(h);
    }
    if (getenv("FG186S_WITNESS")) {
        Dl_info di;
        const char * path = (dladdr(g_target, &di) && di.dli_fname) ? di.dli_fname : "?";
        fprintf(stderr, "FG186S_PATCH target=%p ours=%p lib=%s base=%p\n", g_target, ours, path, di.dli_fbase);
        FILE * m = fopen("/proc/self/maps", "r");
        if (m) {
            char line[512];
            while (fgets(line, sizeof(line), m))
                if (strstr(line, "ggml-cuda")) fprintf(stderr, "FG186S_MAP %s", line);
            fclose(m);
        }
    }
}

__attribute__((destructor)) static void fini(void) {
    if (!g_patched || !g_target) return;
    long page = page_size_ ? page_size_ : sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) g_target & ~(uintptr_t) (page - 1));
    if (mprotect(base, (size_t) page, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        memcpy(g_target, g_saved, sizeof(g_saved));
        __builtin___clear_cache((char *) g_target, (char *) g_target + (int) sizeof(g_saved));
    }
    if (getenv("FG186S_WITNESS")) fprintf(stderr, "FG186S_PATCH restored\n");
}

static void fg186_install_cinit(void * h) {
    void * ci = dlsym(h, "ggml_backend_cuda_init");
    if (!ci) return;
    unsigned char * saved_ci = mmap(NULL, 64, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void * tramp_ci = make_trampoline(ci);
    if (!saved_ci || saved_ci == MAP_FAILED || !tramp_ci) return;
    g_cinit_orig = (cidx_fn) tramp_ci;
    long pg = sysconf(_SC_PAGESIZE);
    void * base = (void *) ((uintptr_t) ci & ~(uintptr_t) (pg - 1));
    if (mprotect(base, (size_t) pg, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return;
    memcpy(saved_ci, ci, 12);
    unsigned char patch[12] = { 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0 };
    uintptr_t dest = (uintptr_t) &cinit_wrap;
    memcpy(&patch[2], &dest, 8);
    memcpy(ci, patch, sizeof(patch));
    __builtin___clear_cache((char *) ci, (char *) ci + sizeof(patch));
    if (getenv("FG186S_WITNESS")) fprintf(stderr, "FG186S_CINIT detour installed\n");
}
