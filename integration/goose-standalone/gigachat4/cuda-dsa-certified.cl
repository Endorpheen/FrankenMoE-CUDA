static uint32_t fg_dsa_int_word(uint64_t address) {
    return *(__global const uint32_t *)address;
}
#define gs_add_u32(a,b) ((a)+(b))
#define gs_shl_u64(a,b) ((a)<<(b))

static float fg_dsa_pack_f16(uint64_t a) { return fg175q_h2f(*((__global const uint16_t *)a)); }

static uint32_t fg_dsa_pack_bits(float x) { return as_uint(x); }

static float fg_dsa_pack_float(uint32_t x) { return as_float(x); }

static uint32_t fg_dsa_pack_xor(uint32_t x, uint32_t delta) {
    uint32_t result;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=r"(result) : "r"(x), "r"(delta));
    return result;
}

static int32_t fg_dsa_pack_rni(float x) {
    int32_t result;
    __asm__("cvt.rni.s32.f32 %0, %1;" : "=r"(result) : "f"(x));
    return result;
}

static void fg_dsa_pack_byte(uint64_t p, int32_t x) { *((__global int8_t *)p) = (int8_t)x; }

static float fg_dsa_integer_to_float(int64_t dot, float a, float b) {
    float x;
    __asm__("cvt.rn.f32.s64 %0, %1;" : "=f"(x) : "l"(dot));
    return fg_device_mul_rn(fg_device_mul_rn(x, a), b);
}

static float fg_dsa_bound_ceil_add(float a, float b) {
    float x;
    __asm__("add.rp.f32 %0, %1, %2;" : "=f"(x) : "f"(a), "f"(b));
    return x;
}

static float fg_dsa_bound_ceil_mul(float a, float b) {
    float x;
    __asm__("mul.rp.f32 %0, %1, %2;" : "=f"(x) : "f"(a), "f"(b));
    return x;
}

static float fg_dsa_bound_xor(float a, int32_t delta) {
    return fg_attn_shfl(a, delta);
}

static void fg_dsa_bound_atomic_max(uint64_t p, uint32_t v) {
    __asm__ volatile("red.global.max.u32 [%0], %1;" :: "l"(p), "r"(v) : "memory");
}

#include "dsa-certified-device.h"

#include "cuda-dsa-entries.h"
