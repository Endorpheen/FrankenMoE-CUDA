#include "rms-reference-types.h"

static float fg168_load(uint64_t p, uint64_t i) { return fg_device_global_load(p + 4 * i); }
static void fg168_store(uint64_t p, uint64_t i, float v) { fg_device_global_store(p + 4 * i, v); }
static RmsVec_g fg168_load4(uint64_t p, int32_t i) {
    const float4 value = ((__global const float4 *) p)[i];
    return (RmsVec_g) {value.x, value.y, value.z, value.w};
}
static void fg168_store4(uint64_t p, int32_t i, RmsVec_g v) {
    ((__global float4 *) p)[i] = (float4) (v.x_g, v.y_g, v.z_g, v.w_g);
}
#include "cuda-rms-reference-ptx-0.h"

#include "rms-reference-device.h"

#include "cuda-rms-reference-entries.h"
