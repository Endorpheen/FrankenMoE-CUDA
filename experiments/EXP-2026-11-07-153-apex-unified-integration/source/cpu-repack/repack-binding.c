// Default-on Goose binding for the active Q4_Kx8 AVX2 repack path.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int fg006_repack_q4_Kx8(void *, const void *, int64_t, int64_t);
extern int fg006_quantize_mat_q8_K_4x8(const float *, void *, int64_t);
extern void fg006_gemv_q4_K_8x8_q8_K(int, float *, size_t, const void *, const void *, int, int);
extern void fg006_gemm_q4_K_8x8_q8_K(int, float *, size_t, const void *, const void *, int, int);
extern void fg006_repack_report(void);

typedef int (*q8_fn)(const float *, void *, int64_t);
typedef void (*gemv_fn)(int, float *, size_t, const void *, const void *, int, int);
static int enabled = 1;
static uint64_t calls[3];

__attribute__((constructor)) static void cfg(void) {
    const char * v = getenv("FG006_REPACK_ROLLBACK");
    enabled = !v || strcmp(v, "1") != 0;
}
void ggml_quantize_mat_q8_K_4x8(const float * x, void * vy, int64_t k) {
    static q8_fn original;
    if (!original) original = (q8_fn) dlsym(RTLD_NEXT, "ggml_quantize_mat_q8_K_4x8");
    if (enabled) { if (fg006_quantize_mat_q8_K_4x8(x, vy, k) == 0) { ++calls[0]; return; } }
    original(x, vy, k);
}
void ggml_gemv_q4_K_8x8_q8_K(int n, float * s, size_t bs, const void * vx, const void * vy, int nr, int nc) {
    static gemv_fn original;
    if (!original) original = (gemv_fn) dlsym(RTLD_NEXT, "ggml_gemv_q4_K_8x8_q8_K");
    if (enabled && n > 0 && n % 256 == 0) { fg006_gemv_q4_K_8x8_q8_K(n, s, bs, vx, vy, nr, nc); ++calls[1]; return; }
    original(n, s, bs, vx, vy, nr, nc);
}
void ggml_gemm_q4_K_8x8_q8_K(int n, float * s, size_t bs, const void * vx, const void * vy, int nr, int nc) {
    static gemv_fn original;
    if (!original) original = (gemv_fn) dlsym(RTLD_NEXT, "ggml_gemm_q4_K_8x8_q8_K");
    if (enabled && n > 0 && n % 256 == 0 && nr % 4 == 0) { fg006_gemm_q4_K_8x8_q8_K(n, s, bs, vx, vy, nr, nc); ++calls[2]; return; }
    original(n, s, bs, vx, vy, nr, nc);
}
uint64_t fg006_binding_count(int i) { return (i >= 0 && i < 3) ? calls[i] : 0; }
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG006_REPACK_BIND enabled=%d q8=%llu gemv=%llu gemm=%llu\n", enabled,
            (unsigned long long) calls[0], (unsigned long long) calls[1], (unsigned long long) calls[2]);
    fg006_repack_report();
}
