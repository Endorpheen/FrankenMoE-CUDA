#pragma OPENCL EXTENSION cl_khr_fp64 : enable
extern float __nv_expf(float);




static float i2d_expf(float value) {
    return __nv_expf(value);
}

static float i2d_negative_infinity(void) {
    return as_float(0xff800000u);
}


#include "attention-image-ptx-0.h"
#include "inkling-attention-device.h"

#include "attention-image-entries.h"
