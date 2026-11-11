// Drop-in replacement for the dispatched kernel. Separate from bridge.c so the gate
// compares the module against the library symbol instead of against itself.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int fg410s_enabled(void);
extern float fg410s_dot(int64_t nb, const void * q4, const void * q8);
/* ---- drop-in replacement, enabled by default, FG410S_ENABLED=0 disables ---- */
#define _GNU_SOURCE
#include <dlfcn.h>
extern float fg410s_dot(int64_t nb, const void * q4, const void * q8);

static int fg410s_switch(void) {
    const char * e = getenv("FG410S_ENABLED");
    return !(e && e[0] == '0');
}
static void fg410s_sync(void * f, int n, float * s, size_t bs, const void * vx, size_t bx,
                        const void * vy, size_t by, int nrc, const char * name) {
    static unsigned long calls = 0;
    const int64_t nb = (int64_t) (n / 256);
    *s = fg410s_dot(nb, (const char *) vx + bx * nrc, (const char *) vy + by * nrc);
    calls++;
    if (getenv("FG410S_WITNESS") && calls == 1)
        fprintf(stderr, "FG410S_WITNESS calls=%lu nb=%lld sym=%s\n", calls, (long long) nb, name);
    (void) f;
}
/* the dispatched symbol is what the server actually calls */
void ggml_vec_dot_q4_K_q8_K(int n, float * s, size_t bs, const void * vx, size_t bx,
                            const void * vy, size_t by, int nrc) {
    static void (*native)(int, float *, size_t, const void *, size_t, const void *, size_t, int) = NULL;
    if (!fg410s_switch() || !fg410s_enabled()) {
        if (!native) native = dlsym(RTLD_NEXT, "ggml_vec_dot_q4_K_q8_K");
        native(n, s, bs, vx, bx, vy, by, nrc);
        return;
    }
    fg410s_sync(NULL, n, s, bs, vx, bx, vy, by, nrc, "dispatch");
}

void ggml_vec_dot_q4_K_q8_K_generic(int n, float * s, size_t bs, const void * vx, size_t bx,
                                    const void * vy, size_t by, int nrc) {
    static int enabled = -1;
    static unsigned long calls = 0;
    if (enabled < 0) {
        const char * e = getenv("FG410S_ENABLED");
        enabled = !(e && e[0] == '0');
    }
    if (!enabled) {
        static void (*native)(int, float *, size_t, const void *, size_t, const void *, size_t, int) = NULL;
        if (!native) native = dlsym(RTLD_NEXT, "ggml_vec_dot_q4_K_q8_K_generic");
        native(n, s, bs, vx, bx, vy, by, nrc);
        return;
    }
    const int64_t nb = (int64_t) (n / 256);
    *s = fg410s_dot(nb, (const char *) vx + bx * nrc, (const char *) vy + by * nrc);
    calls++;
    if (getenv("FG410S_WITNESS") && calls == 1)
        fprintf(stderr, "FG410S_WITNESS calls=%lu nb=%lld\n", calls, (long long) nb);
}
