#include <stdint.h>
#include <string.h>
#include <math.h>
static void g_fail(int64_t c) { (void)c; __builtin_trap(); }
static float g_ld_f32(uint64_t a) { return *(const float *)(uintptr_t)a; }
static void g_st_u8(uint64_t a, int64_t v) { *(uint8_t *)(uintptr_t)a = (uint8_t)v; }
static float g_abs(float v) { return fabsf(v); }
static int64_t g_lround(float v) { return (int64_t) lroundf(v); }
/* fp32 -> fp16 bits, round to nearest even (the reference's GGML_FP32_TO_FP16) */
static void g_st_f16(uint64_t a, float v) {
    uint32_t bits; memcpy(&bits, &v, 4);
    const uint32_t sign = (bits >> 16) & 0x8000u;
    int32_t exp = (int32_t)((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t man = bits & 0x7FFFFFu;
    uint16_t h;
    if (((bits >> 23) & 0xFF) == 0xFF) { h = (uint16_t)(sign | 0x7C00u | (man ? 0x200u : 0)); }
    else if (exp >= 0x1F) { h = (uint16_t)(sign | 0x7C00u); }
    else if (exp <= 0) {
        if (exp < -10) { h = (uint16_t) sign; }
        else {
            man |= 0x800000u;
            const uint32_t shift = (uint32_t)(14 - exp);
            uint32_t sub = man >> shift;
            const uint32_t rem = man & ((1u << shift) - 1);
            const uint32_t half = 1u << (shift - 1);
            if (rem > half || (rem == half && (sub & 1))) sub++;
            h = (uint16_t)(sign | sub);
        }
    } else {
        uint32_t m = man >> 13;
        const uint32_t rem = man & 0x1FFFu;
        if (rem > 0x1000u || (rem == 0x1000u && (m & 1))) { m++; if (m == 0x400u) { m = 0; exp++; if (exp >= 0x1F) { h = (uint16_t)(sign | 0x7C00u); memcpy((void *)(uintptr_t)a, &h, 2); return; } } }
        h = (uint16_t)(sign | ((uint32_t)exp << 10) | m);
    }
    memcpy((void *)(uintptr_t)a, &h, 2);
}
#define fg333q_ld_f32 g_ld_f32
#define fg333q_st_u8  g_st_u8
#define fg333q_st_f16 g_st_f16
#define fg333q_abs    g_abs
#define fg333q_lround g_lround
#define fg333q_fail   g_fail
#define main fg333q_generated_main
#include "enc-generated.c"
#undef main
void fg333q_enter(const float * x, uint8_t * y, int64_t k) { fg333q_fn_1((uint64_t)(uintptr_t)x, (uint64_t)(uintptr_t)y, k, 0); }
