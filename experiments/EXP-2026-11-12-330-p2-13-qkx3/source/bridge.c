#include <stdint.h>
#include <string.h>
#include <math.h>
static void g_fail(int64_t c) { (void)c; __builtin_trap(); }
static float g_ld_f32(uint64_t a) { return *(const float *)(uintptr_t)a; }
static int64_t g_ld_u8(uint64_t a) { return *(const uint8_t *)(uintptr_t)a; }
static void g_st_u8(uint64_t a, int64_t v) { *(uint8_t *)(uintptr_t)a = (uint8_t)v; }
static void g_st_f32(uint64_t a, float v) { memcpy((void *)(uintptr_t)a, &v, 4); }
static float g_abs(float v) { return fabsf(v); }
static float g_neg(float v) { return -v; }
static int64_t g_nearest(float v) { v += 12582912.f; int32_t i; memcpy(&i, &v, 4); return (int64_t)(i & 0x003fffff) - 0x200000; }
#define fg330q_ld_f32  g_ld_f32
#define fg330q_ld_u8   g_ld_u8
#define fg330q_st_u8   g_st_u8
#define fg330q_st_f32  g_st_f32
#define fg330q_nearest g_nearest
#define fg330q_abs     g_abs
#define fg330q_neg     g_neg
#define fg330q_fail    g_fail
#define main fg330q_generated_main
#include "qkx3-generated.c"
#undef main
float fg330q_enter(int64_t n, int64_t nmax, const float * x, const float * w, int64_t hasw,
                   uint8_t * L, float * the_min, uint8_t * Laux,
                   float rmin, float rdelta, int64_t nstep, int64_t use_mad) {
    return fg330q_fn_1(n, nmax, (uint64_t)(uintptr_t)x, (uint64_t)(uintptr_t)w, hasw,
                       (uint64_t)(uintptr_t)L, (uint64_t)(uintptr_t)the_min, (uint64_t)(uintptr_t)Laux,
                       rmin, rdelta, nstep, use_mad, 0);
}
