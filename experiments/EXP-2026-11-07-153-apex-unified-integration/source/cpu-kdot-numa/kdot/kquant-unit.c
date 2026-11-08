// Adapter for the Goose Q4_K/Q6_K x Q8_K dot unit.
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stdlib.h>
static int fg006_kdot_enabled=1;
__attribute__((constructor)) static void fg006_kdot_config(void) {
 const char *v=getenv("GGML_GOOSE_CPU_KDOT");fg006_kdot_enabled=(!v || strcmp(v,"0")!=0);
}
void ggml_vec_dot_q4_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);
void ggml_vec_dot_q6_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);

static float fg143_half(uint16_t v) { return _cvtsh_ss(v); }
static float fg143_from_bits(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }
static atomic_uint_fast64_t kq_calls[2], kq_values[2];
static void fg143_witness(int64_t op, int64_t values) {
    if (op < 0 || op >= 2 || values < 0) __builtin_trap();
    atomic_fetch_add_explicit(kq_calls + op, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(kq_values + op, (uint64_t) values, memory_order_relaxed);
}

#define main fg006_kquant_unused_main
#include "kquant-generated.c"
#undef main

float fg006_dot_q4_K_q8_K(int n, const void * x, const void * y) {
    if (n <= 0 || n % 256 != 0) return 0.0f;
    return gg_function_1(0, n, gg_view((void *) x, (int64_t)(n / 256) * 144),
            gg_view((void *) y, (int64_t)(n / 256) * 292), 0);
}
float fg006_dot_q6_K_q8_K(int n, const void * x, const void * y) {
    if (n <= 0 || n % 256 != 0) return 0.0f;
    return gg_function_1(1, n, gg_view((void *) x, (int64_t)(n / 256) * 210),
            gg_view((void *) y, (int64_t)(n / 256) * 292), 0);
}

void fg006_vec_dot_q4_K_q8_K(int n, float * s, size_t bs, const void * vx, size_t bx,
        const void * vy, size_t by, int nrc) {
    if (!fg006_kdot_enabled) { ggml_vec_dot_q4_K_q8_K(n,s,bs,vx,bx,vy,by,nrc);return; }
    if (n < 0 || n % 256 || nrc != 1 || !s) __builtin_trap();
    *s = fg006_dot_q4_K_q8_K(n, vx, vy);
}
void fg006_vec_dot_q6_K_q8_K(int n, float * s, size_t bs, const void * vx, size_t bx,
        const void * vy, size_t by, int nrc) {
    if (!fg006_kdot_enabled) { ggml_vec_dot_q6_K_q8_K(n,s,bs,vx,bx,vy,by,nrc);return; }
    if (n < 0 || n % 256 || nrc != 1 || !s) __builtin_trap();
    *s = fg006_dot_q6_K_q8_K(n, vx, vy);
}

__attribute__((destructor)) static void fg006_kquant_report(void) {
    for (int i = 0; i < 2; ++i) {
        uint64_t n = atomic_load(kq_calls + i);
        if (n) fprintf(stderr, "FG006_KQUANT mode=%d calls=%llu values=%llu\n",
                i, (unsigned long long) n, (unsigned long long) atomic_load(kq_values + i));
    }
}
