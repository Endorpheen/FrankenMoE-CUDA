#include <stdint.h>
#include <string.h>
#include <math.h>
static void g_fail(int64_t c) { (void)c; __builtin_trap(); }
static float g_ld_f32(uint64_t a) { return *(const float *)(uintptr_t)a; }
static int64_t g_ld_u8(uint64_t a) { return *(const uint8_t *)(uintptr_t)a; }
static void g_st_u16(uint64_t a, int64_t v) { uint16_t h=(uint16_t)v; memcpy((void *)(uintptr_t)a, &h, 2); }
static void g_st_f32(uint64_t a, float v) { memcpy((void *)(uintptr_t)a, &v, 4); }
static int64_t g_nearest(float v) { v += 12582912.f; int32_t i; memcpy(&i, &v, 4); return (int64_t)(i & 0x003fffff) - 0x200000; }
static void g_st_u8(uint64_t a, int64_t v) { *(uint8_t *)(uintptr_t)a = (uint8_t)v; }
static float g_abs(float v) { return fabsf(v); }
/* fp32 -> fp16, round to nearest even (the reference's GGML_FP32_TO_FP16) */
static void g_st_f16(uint64_t a, float v) {
    uint32_t bits; memcpy(&bits, &v, 4);
    uint16_t h;
    const uint32_t sign = (bits >> 16) & 0x8000u;
    const uint32_t e = (bits >> 23) & 0xFFu;
    uint32_t m = bits & 0x7FFFFFu;
    int32_t exp = (int32_t)e - 127 + 15;
    if (e == 0xFF) h = (uint16_t)(sign | 0x7C00u | (m ? 0x200u : 0));
    else if (exp >= 0x1F) h = (uint16_t)(sign | 0x7C00u);
    else if (exp <= 0) {
        if (exp < -10) h = (uint16_t)sign;
        else {
            m |= 0x800000u;
            const uint32_t sh = (uint32_t)(14 - exp);
            uint32_t sub = m >> sh;
            const uint32_t rem = m & ((1u << sh) - 1);
            const uint32_t half = 1u << (sh - 1);
            if (rem > half || (rem == half && (sub & 1))) sub++;
            h = (uint16_t)(sign | sub);
        }
    } else {
        uint32_t mm = m >> 13;
        const uint32_t rem = m & 0x1FFFu;
        if (rem > 0x1000u || (rem == 0x1000u && (mm & 1))) { mm++; if (mm == 0x400u) { mm = 0; exp++; if (exp >= 0x1F) { h = (uint16_t)(sign | 0x7C00u); memcpy((void *)(uintptr_t)a, &h, 2); return; } } }
        h = (uint16_t)(sign | ((uint32_t)exp << 10) | mm);
    }
    memcpy((void *)(uintptr_t)a, &h, 2);
}
#define fg336q_ld_f32 g_ld_f32
#define fg336q_ld_u8  g_ld_u8
#define fg336q_st_u16 g_st_u16
#define fg336q_st_f32 g_st_f32
#define fg336q_nearest g_nearest
#define fg336q_st_u8  g_st_u8
#define fg336q_st_f16 g_st_f16
#define fg336q_abs    g_abs
#define fg336q_fail   g_fail
#define main fg336q_generated_main
#include "enc-generated.c"
#undef main
static uint8_t g_scratch[512];
void fg336q_enter(const float * x, uint8_t * y, int64_t k) {
    fg336q_fn_1((uint64_t)(uintptr_t)x, (uint64_t)(uintptr_t)y, k, (uint64_t)(uintptr_t)g_scratch, 0);
}
