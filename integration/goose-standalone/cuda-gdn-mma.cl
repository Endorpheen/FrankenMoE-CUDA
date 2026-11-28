#include "gdn-mma-types.h"

static float fg174m_load(uint64_t p, uint64_t i) { return fg_device_global_load(p + 4 * i); }
static void fg174m_store(uint64_t p, uint64_t i, float v) { fg_device_global_store(p + 4 * i, v); }
static F4_g fg174m_load4(uint64_t p, uint64_t i) {
    const float4 v = *(__global const float4 *) (p + 4 * i);
    return (F4_g) {v.x, v.y, v.z, v.w};
}
static float fg174m_shfl_up(float v, int32_t delta) {
    uint32_t r;
    __asm__ volatile("shfl.sync.up.b32 %0, %1, %2, 0, -1;" : "=r"(r) : "r"(as_uint(v)), "r"(delta));
    return as_float(r);
}
static float fg174m_shfl(float v, int32_t lane) {
    uint32_t r;
    __asm__ volatile("shfl.sync.idx.b32 %0, %1, %2, 31, -1;" : "=r"(r) : "r"(as_uint(v)), "r"(lane));
    return as_float(r);
}
static float fg174m_sm_bf16(uint64_t p, uint64_t i) {
    return as_float((uint32_t) ((__local uint16_t *) p)[i] << 16);
}
static void fg174m_sm_store_bf16(uint64_t p, uint64_t i, float v) {
    uint16_t bits;
    __asm__("cvt.rn.bf16.f32 %0, %1;" : "=h"(bits) : "f"(v));
    ((__local uint16_t *) p)[i] = bits;
}
#include "cuda-gdn-mma-ptx-0.h"
static float fg174m_wlo(uint32_t w) { return as_float(w << 16); }
static float fg174m_whi(uint32_t w) { return as_float(w & 0xffff0000u); }

#include "cuda-gdn-mma-matrix-ptx-0.h"
#include "gdn-mma-device.h"

#include "cuda-gdn-mma-entries.h"
