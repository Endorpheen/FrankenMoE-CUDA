#include <stdint.h>
#include <string.h>
#include <math.h>
static void g_fail(int64_t c) { (void)c; __builtin_trap(); }
static float g_ld_f32(uint64_t a) { return *(const float *)(uintptr_t)a; }
static void g_st_u8(uint64_t a, int64_t v) { *(uint8_t *)(uintptr_t)a = (uint8_t)v; }
static float g_abs(float v) { return fabsf(v); }
static float g_ue4m3(int64_t ev) {
    const uint8_t x = (uint8_t) ev;
    if (x == 0 || x == 0x7F) return 0.0f;
    const int exp = (x >> 3) & 0xF, man = x & 0x7;
    const float raw = (exp == 0) ? ldexpf((float) man, -9) : ldexpf(1.0f + (float) man / 8.0f, exp - 7);
    return raw * 0.5f;
}
static int64_t g_to_ue4m3(float x) {
    if (!(x > 0.0f)) return 0;
    if (x > 448.0f) x = 448.0f;
    uint32_t bits; memcpy(&bits, &x, 4);
    int fp32_exp = (int)((bits >> 23) & 0xFF) - 127;
    int fp32_man = (int)((bits >> 20) & 0x7);
    int ue4m3_exp = fp32_exp + 7;
    if (ue4m3_exp <= 0) {
        int man = (int)(x * 512.0f + 0.5f);
        if (man > 7) man = 7;
        if (man < 1) return 0;
        return man;
    }
    if (ue4m3_exp >= 15) return 0x7E;
    int round_bit = (int)((bits >> 19) & 1);
    int ue4m3_man = fp32_man + round_bit;
    if (ue4m3_man > 7) { ue4m3_man = 0; ue4m3_exp++; if (ue4m3_exp >= 15) return 0x7E; }
    return (ue4m3_exp << 3) | ue4m3_man;
}
#define fg332q_ld_f32   g_ld_f32
#define fg332q_st_u8    g_st_u8
#define fg332q_abs      g_abs
#define fg332q_ue4m3    g_ue4m3
#define fg332q_to_ue4m3 g_to_ue4m3
#define fg332q_fail     g_fail
#define main fg332q_generated_main
#include "enc-generated.c"
#undef main
void fg332q_enter(const float * x, uint8_t * y, int64_t k) { fg332q_fn_1((uint64_t)(uintptr_t)x, (uint64_t)(uintptr_t)y, k, 0); }
