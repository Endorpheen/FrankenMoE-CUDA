#include <stdint.h>
#include <string.h>

static void g_fail(int64_t c) { (void)c; __builtin_trap(); }
static int64_t g_ld_u8(uint64_t a) { return *(const uint8_t *)(uintptr_t)a; }
static int64_t g_ld_i8(uint64_t a) { return (int64_t)*(const int8_t *)(uintptr_t)a; }
static float g_ld_f16(uint64_t a) {
    uint16_t h; memcpy(&h, (const void *)(uintptr_t)a, 2);
    const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    const uint32_t exp  = (h >> 10) & 0x1fu;
    uint32_t mant       = (uint32_t)(h & 0x3ffu) << 13;
    uint32_t bits;
    if (exp == 0) {
        if (mant == 0) { bits = sign; }
        else { int e = -1; while (!(mant & 0x800000u)) { mant <<= 1; e++; } mant &= 0x7fffffu;
               bits = sign | (uint32_t)((127 - 15 - e) << 23) | mant; }
    } else if (exp == 31) { bits = sign | 0x7f800000u | mant; }
    else { bits = sign | (uint32_t)((exp + 127 - 15) << 23) | mant; }
    float f; memcpy(&f, &bits, 4); return f;
}
static void g_st_f32(uint64_t a, float v) { memcpy((void *)(uintptr_t)a, &v, 4); }

#define fg319q_ld_u8  g_ld_u8
#define fg319q_ld_i8  g_ld_i8
#define fg319q_ld_f16 g_ld_f16
#define fg319q_st_f32 g_st_f32
#define fg319q_fail   g_fail
#define main fg319q_generated_main
#include "tq1-generated.c"
#undef main

void fg319q_decode_ref(const void * src, float * dst, int64_t k) {
    fg319q_fn_1((uint64_t)(uintptr_t)src, (uint64_t)(uintptr_t)dst, k, 0);
}
