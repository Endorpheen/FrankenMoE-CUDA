typedef int int32_t;
typedef long int64_t;
typedef uchar uint8_t;
typedef ulong uint64_t;
#define gs_add_i32(a,b,file,line) ((a)+(b))
static float route_raw(ulong address, int index) { return ((__global float *)address)[index]; }
static void route_soft_store(ulong address, int index, float value) { ((__global float *)address)[index] = value; }
static uint route_shuffle(uint input, int offset) {
    uint output;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=r"(output) : "r"(input), "r"(offset));
    return output;
}
static float route_f32_xor(float input, int offset) { return as_float(route_shuffle(as_uint(input), offset)); }
static int route_swap_i32(int input, int offset) { return as_int(route_shuffle(as_uint(input), offset)); }
static double route_f64_xor(double input, int offset) {
    ulong bits = as_ulong(input);
    uint low = route_shuffle((uint)bits, offset);
    uint high = route_shuffle((uint)(bits >> 32), offset);
    return as_double(((ulong)high << 32) | low);
}
static float route_max(float a, float b) {
    float value;
    __asm__("max.f32 %0, %1, %2;" : "=f"(value) : "f"(a), "f"(b));
    return value;
}
static float route_exponent(float input) {
    float scaled, result;
    __asm__("mul.rn.f32 %0, %1, 0f3FB8AA3B;" : "=f"(scaled) : "f"(input));
    __asm__("ex2.approx.ftz.f32 %0, %1;" : "=f"(result) : "f"(scaled));
    return result;
}
static void route_fail(void) { __asm__("trap;"); }
#include "softmax-device.h"
__kernel void goose_router_softmax_fixed(__global float *input, __global float *output, float scale) {
    int lane;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    goose_router_softmax_256_g((ulong)input, (ulong)output, scale, lane);
}
