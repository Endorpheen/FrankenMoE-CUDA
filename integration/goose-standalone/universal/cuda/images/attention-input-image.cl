#pragma OPENCL EXTENSION cl_khr_fp64 : enable
extern float __nv_sqrtf(float);
extern float __nv_logf(float);

#include "attention-input-image-ptx-0.h"
static void input_half(uint64_t base, int64_t i, float value) {
    ushort bits;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(bits) : "f"(value));
    ((__global ushort *)base)[i] = bits;
}
#include "i2-attention-input-device.h"

#include "attention-input-image-entries.h"
