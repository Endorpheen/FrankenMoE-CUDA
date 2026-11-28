typedef struct { int32_t a_g, b_g, c_g, d_g; } MmqDot_g;
typedef struct { float a_g, b_g, c_g, d_g; } MmqSum_g;
typedef struct { uint32_t a_g, b_g, c_g, d_g; } MmqWords_g;
typedef struct { float d0_g, d1_g, m0_g, m1_g; } MmqScales_g;
typedef struct { int32_t a_g, b_g, c_g, d_g; } MmqQ6Scales_g;
typedef struct { MmqSum_g left_g, right_g; } MmqPairSum_g;
typedef struct { int64_t work_g, workers_g, start_g; } MmqPartition_g;
typedef struct { int64_t route_g, live_g; uint64_t group_g, col0_g, col1_g; } MmqOperand_g;
#define gs_shl_i32(a, b) as_int((uint32_t) (a) << ((b) & 31))

static uint32_t fg_mmq_aligned_word(uint64_t address) { return *(__global uint32_t *)address; }
static uint32_t fg_mmq_half_word(uint64_t address) { return *(__global uint16_t *)address; }

static uint64_t fg_mmq_index(uint64_t address, int64_t index) {
    return ((__global const uint64_t *) address)[index];
}

static uint16_t mmq_half_bits(float value) {
    uint16_t bits;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(bits) : "f"(value));
    return bits;
}

static float fg_mmq_half_round(float value) {
    const uint16_t bits = mmq_half_bits(value);
    float rounded;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(rounded) : "h"(bits));
    return rounded;
}

static MmqDot_g fg_mmq_mma32(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                            uint32_t b0, uint32_t b1) {
    MmqDot_g r;
    __asm__ volatile("mma.sync.aligned.m16n8k32.row.col.s32.s8.s8.s32 "
                     "{%0, %1, %2, %3}, {%4, %5, %6, %7}, {%8, %9}, {%10, %10, %10, %10};"
                     : "=r"(r.a_g), "=r"(r.b_g), "=r"(r.c_g), "=r"(r.d_g)
                     : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1), "r"(0u));
    return r;
}

static MmqDot_g fg_mmq_mma16(uint32_t a0, uint32_t a1, uint32_t b0) {
    MmqDot_g r;
    __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.s32.s8.s8.s32 "
                     "{%0, %1, %2, %3}, {%4, %5}, {%6}, {%7, %7, %7, %7};"
                     : "=r"(r.a_g), "=r"(r.b_g), "=r"(r.c_g), "=r"(r.d_g)
                     : "r"(a0), "r"(a1), "r"(b0), "r"(0u));
    return r;
}

static MmqWords_g fg_mmq_ldm32(uint64_t address) {
    MmqWords_g words;
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x4.shared.b16 {%0, %1, %2, %3}, [%4];"
        : "=r"(words.a_g), "=r"(words.b_g), "=r"(words.c_g), "=r"(words.d_g)
        : "r"((uint32_t) address) : "memory");
    return words;
}

static MmqWords_g fg_mmq_ldm16(uint64_t address) {
    MmqWords_g words = {0};
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x2.shared.b16 {%0, %1}, [%2];"
        : "=r"(words.a_g), "=r"(words.b_g) : "r"((uint32_t) address) : "memory");
    return words;
}

static void fg_mmq_shared_store(uint64_t address, uint32_t word) {
    *(__local uint32_t *) address = word;
}
static void fg_mmq_global_store_word(uint64_t address, uint32_t word) {
    *(__global uint32_t *) address = word;
}

static float fg_mmq_shared_float(uint64_t address) { return *(__local float *) address; }
static int32_t fg_mmq_shared_i8(uint64_t address) { return *(__local int8_t *) address; }
static void fg_mmq_shared_store_float(uint64_t address, float value) { *(__local float *) address = value; }
static uint32_t fg_mmq_shared_word(uint64_t address) { return *(__local uint32_t *) address; }
static float fg_mmq_half_value(uint32_t bits) {
    float value;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(value) : "h"((uint16_t) bits));
    return value;
}
static void fg_mmq_copy_16(uint64_t dst, uint64_t src) {
    *(__local uint4 *) dst = *(__global const uint4 *) src;
}


static float fg180q_ld_f4x(uint64_t src, uint64_t off) { return fg_device_global_load(src + off); }
static float fg180q_ld_f4y(uint64_t src, uint64_t off) { return fg_device_global_load(src + off + 4); }
static float fg180q_ld_f4z(uint64_t src, uint64_t off) { return fg_device_global_load(src + off + 8); }
static float fg180q_ld_f4w(uint64_t src, uint64_t off) { return fg_device_global_load(src + off + 12); }
static int64_t fg180q_ld_i32(uint64_t src, uint64_t off) { return *(__global int32_t *) (src + off); }
static void fg180q_st_i32(uint64_t dst, uint64_t off, int32_t value) { *(__global int32_t *) (dst + off) = value; }
static void fg180q_st_f32(uint64_t dst, uint64_t off, float value) { fg_device_global_store(dst + off, value); }
static void fg180q_st_half2(uint64_t dst, uint64_t off, float d, float sum) {
    __global uint16_t * output = (__global uint16_t *) (dst + off);
    output[0] = mmq_half_bits(d);
    output[1] = mmq_half_bits(sum);
}
static float fg180q_shfl_xor(float value, uint32_t mask, int32_t delta) {
    uint32_t result;
    __asm__ volatile("shfl.sync.bfly.b32 %0, %1, %2, 31, %3;"
                     : "=r"(result) : "r"(as_uint(value)), "r"(delta), "r"(mask));
    return as_float(result);
}
static int32_t fg180q_i32(float value) {
    int32_t result;
    __asm__("cvt.rzi.s32.f32 %0, %1;" : "=r"(result) : "f"(value));
    return result;
}
static float fg180q_round(float value) {
    if ((as_uint(value) & 0x7f800000u) == 0x7f800000u) return 0.0f;
    const int32_t integral = fg180q_i32(value);
    const float rest = value - (float) integral;
    return (float) (integral + (rest >= 0.5f) - (rest <= -0.5f));
}
static float fg180q_fabs(float value) { return as_float(as_uint(value) & 0x7fffffffu); }
static void fg180q_pdl_sync(void) {}


#include "mmq-quant-types.h"
#include "mmq-quant-device.h"
#include "mmq-device.h"

#include "cuda-mmq-entries.h"
