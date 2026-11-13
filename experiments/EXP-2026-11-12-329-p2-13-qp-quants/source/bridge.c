#include <stdint.h>
#include <string.h>
#include <math.h>
static void g_fail(int64_t c) { (void)c; __builtin_trap(); }
static float g_ld_f32(uint64_t a) { return *(const float *)(uintptr_t)a; }
static int64_t g_ld_u8(uint64_t a) { return *(const uint8_t *)(uintptr_t)a; }
static void g_st_u8(uint64_t a, int64_t v) { *(uint8_t *)(uintptr_t)a = (uint8_t)v; }
/* the reference's nearest_int: add 12582912.f and drop the mantissa bits (round half to even) */
static int64_t g_nearest(float v) {
    v += 12582912.f;
    int32_t i; memcpy(&i, &v, 4);
    return (int64_t)(i & 0x007fffff) - 0x00400000;
}
#define fg329q_ld_f32  g_ld_f32
#define fg329q_ld_u8   g_ld_u8
#define fg329q_st_u8   g_st_u8
#define fg329q_nearest g_nearest
#define fg329q_fail    g_fail
#define main fg329q_generated_main
#include "qp-generated.c"
#undef main
float fg329q_enter(int64_t n, int64_t nmax, const float * x, uint8_t * L, const float * w) {
    return fg329q_fn_1(n, nmax, (uint64_t)(uintptr_t) x, (uint64_t)(uintptr_t) L, (uint64_t)(uintptr_t) w, 0);
}
