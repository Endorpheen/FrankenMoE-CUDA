#include <stdint.h>
#include <string.h>
#define GGML_COMMON_DECL_C
#define GGML_COMMON_IMPL_C
#include "ggml-common.h"

static void g_fail(int64_t c) { (void)c; __builtin_trap(); }
static int64_t g_ld_u8(uint64_t a) { return *(const uint8_t *)(uintptr_t)a; }
static int64_t g_ld_i8(uint64_t a) { return (int64_t)*(const int8_t *)(uintptr_t)a; }
static int64_t g_ld_u16(uint64_t a) { uint16_t v; memcpy(&v, (const void *)(uintptr_t)a, 2); return (int64_t)v; }
static int64_t g_ld_u32(uint64_t a) { uint32_t v; memcpy(&v, (const void *)(uintptr_t)a, 4); return (int64_t)v; }
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
static int64_t g_grid(int64_t code) { return (int64_t) iq2xs_grid[code]; }
static int64_t g_signs(int64_t idx) { return (int64_t) ksigns_iq2xs[idx]; }
static int64_t g_kmask(int64_t j) { return (int64_t) kmask_iq2xs[j]; }

#define fg321q_ld_u8  g_ld_u8
#define fg321q_ld_u16 g_ld_u16
#define fg321q_ld_i8  g_ld_i8
#define fg321q_ld_u32 g_ld_u32
#define fg321q_ld_f16 g_ld_f16
#define fg321q_st_f32 g_st_f32
#define fg321q_grid   g_grid
#define fg321q_signs  g_signs
#define fg321q_kmask  g_kmask
#define fg321q_fail   g_fail
#define main fg321q_generated_main
#include "iq2xs-generated.c"
#undef main

void fg321q_decode_ref(const void * src, float * dst, int64_t k) {
    fg321q_fn_1((uint64_t)(uintptr_t)src, (uint64_t)(uintptr_t)dst, k, 0);
}
