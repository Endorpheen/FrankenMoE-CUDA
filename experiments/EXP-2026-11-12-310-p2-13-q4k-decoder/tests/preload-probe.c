#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>
typedef void (*fn_t)(const void *, float *, int64_t);
static unsigned long long g_calls;
void dequantize_row_q4_K(const void * x, float * y, int64_t k) {
    static fn_t real;
    if (!real) real = (fn_t) dlsym(RTLD_NEXT, "dequantize_row_q4_K");
    if (g_calls++ == 0) fprintf(stderr, "FG310Q_PRELOAD first call k=%lld\n", (long long) k);
    real(x, y, k);
}
__attribute__((destructor)) static void report(void) { fprintf(stderr, "FG310Q_PRELOAD calls=%llu\n", g_calls); }
