typedef uint uint32_t;
typedef int int32_t;
typedef long int64_t;
typedef uchar uint8_t;
typedef ulong uint64_t;
#define gs_add_i32(a,b,file,line) ((a)+(b))
#include "warp-shuffle.h"
#include "softmax-ptx-0.h"
static float route_exponent(float input) {
    float scaled, result;
    __asm__("mul.rn.f32 %0, %1, 0f3FB8AA3B;" : "=f"(scaled) : "f"(input));
    __asm__("ex2.approx.ftz.f32 %0, %1;" : "=f"(result) : "f"(scaled));
    return result;
}
#include "softmax-ptx-1.h"
#include "softmax-device.h"
__kernel void goose_router_softmax_fixed(__global float *input, __global float *output, float scale) {
    int lane;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    goose_router_softmax_256_g((ulong)input, (ulong)output, scale, lane);
}
