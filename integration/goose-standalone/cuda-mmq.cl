typedef struct { int32_t a_g, b_g, c_g, d_g; } MmqDot_g;
typedef struct { float a_g, b_g, c_g, d_g; } MmqSum_g;
typedef struct { uint32_t a_g, b_g, c_g, d_g; } MmqWords_g;
typedef struct { float d0_g, d1_g, m0_g, m1_g; } MmqScales_g;
typedef struct { int32_t a_g, b_g, c_g, d_g; } MmqQ6Scales_g;
typedef struct { MmqSum_g left_g, right_g; } MmqPairSum_g;
typedef struct { int64_t work_g, workers_g, start_g; } MmqPartition_g;
typedef struct { int64_t route_g, live_g; uint64_t group_g, col0_g, col1_g; } MmqOperand_g;
#define gs_shl_i32(a, b) as_int((uint32_t) (a) << ((b) & 31))



static float fg_mmq_half_round(float value) {
    const uint16_t bits = mmq_half_bits(value);
    float rounded;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(rounded) : "h"(bits));
    return rounded;
}

#include "cuda-mmq-matrix-ptx-0.h"
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
#include "cuda-mmq-ptx-1.h"
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
