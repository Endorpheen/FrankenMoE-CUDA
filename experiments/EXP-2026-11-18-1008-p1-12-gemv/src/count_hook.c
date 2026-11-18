// Per-symbol call counters in the isolated copy of the accepted CPU backend.
// The function code is never modified: only the GOT slot of each candidate is
// redirected to a stub that counts and then calls the original through the
// pointer it replaced. A symbol whose slot is never hit is genuinely not called.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

typedef void (*gemv_fn)(int, float *, size_t, const void *, const void *, int, int);
typedef void (*dot_fn)(int, float *, size_t, const void *, size_t, const void *, size_t, int);

typedef int (*tiled_fn)(const void *, void *, int64_t, int64_t, const int32_t *, char *);
enum { C_GEMV8X8, C_GEMM8X8, C_GEMV8X4, C_GEMM8X4, C_VECDOT, C_TILED, C_DENSE_TILED, C_CANARY_BARRIER, C_CANARY_ROWSIZE, C_N };
static const char * g_names[C_N] = {
    "ggml_gemv_q4_K_8x8_q8_K",
    "ggml_gemm_q4_K_8x8_q8_K",
    "ggml_gemv_q4_K_8x4_q8_K",
    "ggml_gemm_q4_K_8x4_q8_K",
    "ggml_vec_dot_q4_K_q8_K",
    "ggml_compute_forward_mul_mat_id_tiled",
    "ggml_compute_forward_mul_mat_tiled",
    "ggml_barrier",
    "ggml_row_size",
};
static void * g_orig[C_N];
static unsigned long g_count[C_N];
static int g_seen[C_N];

static void note(int id) {
    g_count[id]++;
    if (!g_seen[id]) {
        g_seen[id] = 1;
        if (getenv("FG_COUNT_WITNESS")) fprintf(stderr, "COUNT_FIRST %s\n", g_names[id]);
    }
}

static void c_gemv8x8(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc) {
    note(C_GEMV8X8); ((gemv_fn) g_orig[C_GEMV8X8])(n, s, bs, vx, vy, nr, nc);
}
static void c_gemm8x8(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc) {
    note(C_GEMM8X8); ((gemv_fn) g_orig[C_GEMM8X8])(n, s, bs, vx, vy, nr, nc);
}
static void c_gemv8x4(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc) {
    note(C_GEMV8X4); ((gemv_fn) g_orig[C_GEMV8X4])(n, s, bs, vx, vy, nr, nc);
}
static void c_gemm8x4(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc) {
    note(C_GEMM8X4); ((gemv_fn) g_orig[C_GEMM8X4])(n, s, bs, vx, vy, nr, nc);
}
static void c_vecdot(int n, float *s, size_t bs, const void *vx, size_t bx, const void *vy, size_t by, int nrc) {
    note(C_VECDOT); ((dot_fn) g_orig[C_VECDOT])(n, s, bs, vx, bx, vy, by, nrc);
}
static void c_tiled(const void *p, void *t, int64_t e, int64_t c, const int32_t *r, char *s) {
    note(C_TILED); ((tiled_fn) g_orig[C_TILED])(p, t, e, c, r, s);
}
static void c_dense_tiled(const void *p, void *t, int64_t e, int64_t c, const int32_t *r, char *s) {
    note(C_DENSE_TILED); ((tiled_fn) g_orig[C_DENSE_TILED])(p, t, e, c, r, s);
}
static void c_barrier(void * p) { note(C_CANARY_BARRIER); ((void (*)(void *)) g_orig[C_CANARY_BARRIER])(p); }
static size_t c_rowsize(int t, int64_t ne) { note(C_CANARY_ROWSIZE); return ((size_t (*)(int, int64_t)) g_orig[C_CANARY_ROWSIZE])(t, ne); }
static void * g_stub[C_N];

static int find_slots(struct dl_phdr_info *info, size_t sz, void *data) {
    (void) sz; (void) data;
    if (!info->dlpi_name || !strstr(info->dlpi_name, "libggml-cpu")) return 0;
    const ElfW(Dyn) * dyn = NULL;
    for (int i = 0; i < info->dlpi_phnum; ++i)
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC)
            dyn = (const ElfW(Dyn) *) (info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    if (!dyn) return 0;
    const ElfW(Sym) * symtab = NULL; const char * strtab = NULL;
    const ElfW(Rela) * jmprel = NULL; size_t pltrelsz = 0;
    for (const ElfW(Dyn) * d = dyn; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_SYMTAB: symtab = (const ElfW(Sym) *) d->d_un.d_ptr; break;
            case DT_STRTAB: strtab = (const char *) d->d_un.d_ptr; break;
            case DT_JMPREL: jmprel = (const ElfW(Rela) *) d->d_un.d_ptr; break;
            case DT_PLTRELSZ: pltrelsz = d->d_un.d_val; break;
        }
    }
    if (!symtab || !strtab || !jmprel) return 0;
    const size_t nrel = pltrelsz / sizeof(ElfW(Rela));
    for (size_t i = 0; i < nrel; ++i) {
        const ElfW(Rela) * r = &jmprel[i];
        if (ELF64_R_TYPE(r->r_info) != R_X86_64_JUMP_SLOT) continue;
        const char * name = strtab + symtab[ELF64_R_SYM(r->r_info)].st_name;
        for (int c = 0; c < C_N; ++c) {
            if (strcmp(name, g_names[c]) != 0) continue;
            void ** slot = (void **) (info->dlpi_addr + r->r_offset);
            if (!g_orig[c]) g_orig[c] = *slot;
            if (g_orig[c]) *slot = g_stub[c];
        }
    }
    return 1;
}

__attribute__((constructor)) static void init(void) {
    g_stub[C_GEMV8X8] = (void *) c_gemv8x8;
    g_stub[C_GEMM8X8] = (void *) c_gemm8x8;
    g_stub[C_GEMV8X4] = (void *) c_gemv8x4;
    g_stub[C_GEMM8X4] = (void *) c_gemm8x4;
    g_stub[C_VECDOT]  = (void *) c_vecdot;
    g_stub[C_TILED]   = (void *) c_tiled;
    g_stub[C_DENSE_TILED] = (void *) c_dense_tiled;
    g_stub[C_CANARY_BARRIER] = (void *) c_barrier;
    g_stub[C_CANARY_ROWSIZE] = (void *) c_rowsize;
    dl_iterate_phdr(find_slots, NULL);
    if (getenv("FG_COUNT_WITNESS")) {
        for (int c = 0; c < C_N; ++c)
            fprintf(stderr, "COUNT_INSTALLED %-28s slot=%s\n", g_names[c], g_orig[c] ? "patched" : "absent");
    }
}

__attribute__((destructor)) static void fini(void) {
    for (int c = 0; c < C_N; ++c)
        fprintf(stderr, "COUNT_TOTAL %-28s %lu\n", g_names[c], g_count[c]);
}
