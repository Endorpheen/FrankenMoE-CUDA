#include <stdint.h>
#include <string.h>
#include <math.h>
static void g_fail(int64_t c) { (void)c; __builtin_trap(); }
static float g_ld_f32(uint64_t a) { return *(const float *)(uintptr_t)a; }
static void g_st_u8(uint64_t a, int64_t v) { *(uint8_t *)(uintptr_t)a = (uint8_t)v; }
static float g_abs(float v) { return fabsf(v); }
static float g_floor(float v) { return floorf(v); }
static float g_log2(float v) { return log2f(v); }
static float g_e8m0half(int64_t e) {
    const uint8_t x = (uint8_t) e;
    uint32_t bits = (x < 2) ? (0x00200000u << x) : ((uint32_t)(x - 1) << 23);
    float f; memcpy(&f, &bits, 4); return f;
}
#define fg331q_ld_f32   g_ld_f32
#define fg331q_st_u8    g_st_u8
#define fg331q_abs      g_abs
#define fg331q_floor    g_floor
#define fg331q_log2     g_log2
#define fg331q_e8m0half g_e8m0half
#define fg331q_fail     g_fail
#define main fg331q_generated_main
#include "enc-generated.c"
#undef main
void fg331q_enter(const float * x, uint8_t * y, int64_t k) {
    fg331q_fn_1((uint64_t)(uintptr_t)x, (uint64_t)(uintptr_t)y, k, 0);
}
