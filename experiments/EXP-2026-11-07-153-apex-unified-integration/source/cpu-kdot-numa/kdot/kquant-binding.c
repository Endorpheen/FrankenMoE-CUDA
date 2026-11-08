// Default-on type-trait vec_dot binding for active Apex Q4_K_M.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern float fg006_dot_q4_K_q8_K(int n, const void * x, const void * y);
extern float fg006_dot_q6_K_q8_K(int n, const void * x, const void * y);

typedef void (*dot_fn)(int, float *, size_t, const void *, size_t, const void *, size_t, int);
static int enabled = 1;
static uint64_t calls[2];

__attribute__((constructor)) static void cfg(void) {
    const char * v = getenv("FG006_NATIVE_ROLLBACK");
    enabled = !v || strcmp(v, "1") != 0;
}

void ggml_vec_dot_q4_K_q8_K(int n, float * s, size_t bs, const void * vx, size_t bx,
        const void * vy, size_t by, int nrc) {
    static dot_fn original;
    if (!original) original = (dot_fn) dlsym(RTLD_NEXT, "ggml_vec_dot_q4_K_q8_K");
    if (enabled) { *s = fg006_dot_q4_K_q8_K(n, vx, vy); ++calls[0]; return; }
    original(n, s, bs, vx, bx, vy, by, nrc);
}
void ggml_vec_dot_q6_K_q8_K(int n, float * s, size_t bs, const void * vx, size_t bx,
        const void * vy, size_t by, int nrc) {
    static dot_fn original;
    if (!original) original = (dot_fn) dlsym(RTLD_NEXT, "ggml_vec_dot_q6_K_q8_K");
    if (enabled) { *s = fg006_dot_q6_K_q8_K(n, vx, vy); ++calls[1]; return; }
    original(n, s, bs, vx, bx, vy, by, nrc);
}
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG006_KDOT enabled=%d q4=%llu q6=%llu\n", enabled,
            (unsigned long long) calls[0], (unsigned long long) calls[1]);
}
