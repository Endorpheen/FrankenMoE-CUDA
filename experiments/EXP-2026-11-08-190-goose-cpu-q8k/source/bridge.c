// Host wrapper for the generated Goose module: supplies the extern primitives and the
// single entry point the fixture calls. Nothing here is algorithmic.
#include <stdint.h>
#include <string.h>
#include <math.h>

static float g_ld_f32(uint64_t a) { float v; memcpy(&v, (const void *)(uintptr_t)a, 4); return v; }
static void  g_st_f32(uint64_t a, float v) { memcpy((void *)(uintptr_t)a, &v, 4); }
static void  g_st_i8 (uint64_t a, int32_t v) { int8_t b = (int8_t)v; memcpy((void *)(uintptr_t)a, &b, 1); }
static void  g_st_i16(uint64_t a, int32_t v) { int16_t b = (int16_t)v; memcpy((void *)(uintptr_t)a, &b, 2); }
static int32_t g_ld_i8(uint64_t a) { int8_t b; memcpy(&b, (const void *)(uintptr_t)a, 1); return b; }
// nearest_int is NOT roundf: native adds 12582912 (1.5*2^23) and masks the mantissa,
// which is round-half-to-even via the FP32 representation. Using roundf gives
// round-half-away-from-zero and diverges on exact .5 cases.
static int32_t g_round(float v) {
    float val = v + 12582912.f;
    int32_t i; memcpy(&i, &val, sizeof(i));
    return (i & 0x007fffff) - 0x00400000;
}
static float g_abs(float v) { return fabsf(v); }
static void  g_fail(int64_t c) { (void)c; __builtin_trap(); }

// prototypes for the extern primitives the generated body calls
static float   g_ld_f32(uint64_t a);
static void    g_st_f32(uint64_t a, float v);
static void    g_st_i8 (uint64_t a, int32_t v);
static void    g_st_i16(uint64_t a, int32_t v);
static int32_t g_ld_i8(uint64_t a);
static int32_t g_round(float v);
static float   g_abs(float v);
static void    g_fail(int64_t c);
#define fg190q_ld_f32 g_ld_f32
#define fg190q_st_f32 g_st_f32
#define fg190q_st_i8  g_st_i8
#define fg190q_st_i16 g_st_i16
#define fg190q_ld_i8  g_ld_i8
#define fg190q_round  g_round
#define fg190q_abs    g_abs
#define fg190q_fail   g_fail
#define main fg190q_generated_main
#include "../../../runtime/exp190/build/device-generated.c"
#undef main

uint8_t fg190q_enter(int64_t k, const float * src, void * dst) {
    (void)g_fail;
    fg190q_fn_1(k, (uint64_t)(uintptr_t)src, (uint64_t)(uintptr_t)dst, 0);
    return 1;
}
