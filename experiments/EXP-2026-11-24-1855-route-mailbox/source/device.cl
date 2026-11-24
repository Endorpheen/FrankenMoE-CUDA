typedef float float32_t;
typedef uint uint32_t;
typedef int int32_t;
typedef ulong uint64_t;
typedef long int64_t;
typedef uchar uint8_t;

#define gs_add_i32(a,b,file,line) ((a)+(b))
#define gs_mul_i32(a,b,file,line) ((a)*(b))
#define gs_shr_i32(a,b) ((a)>>(b))
#define GS_RANGE(a,low,high,type,file,line) (a)

static float route_score(ulong address, int index) { return ((__global const float *)address)[index]; }
static void route_write_score(ulong address, int index, float value) { ((__global float *)address)[index] = value; }
static void route_write_id(ulong address, int index, int value) { ((__global int *)address)[index] = value; }
static uint route_shuffle(uint input, int offset) {
    uint output;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=r"(output) : "r"(input), "r"(offset));
    return output;
}
static float route_swap_f32(float input, int offset) { return as_float(route_shuffle(as_uint(input), offset)); }
static int route_swap_i32(int input, int offset) { return as_int(route_shuffle(as_uint(input), offset)); }
static double route_swap_f64(double input, int offset) {
    ulong bits = as_ulong(input);
    uint low = route_shuffle((uint)bits, offset);
    uint high = route_shuffle((uint)(bits >> 32), offset);
    return as_double(((ulong)high << 32) | low);
}
static float route_neg_inf(void) { return as_float(0xff800000u); }
static float route_clamp(float input) {
    float output;
    __asm__("max.f32 %0, %1, 0f38800000;" : "=f"(output) : "f"(input));
    return output;
}
static float route_scaled(float weight, float scale) {
    float product, output;
    __asm__("mul.rn.f32 %0, %1, %2;" : "=f"(product) : "f"(weight), "f"(scale));
    __asm__("add.rn.f32 %0, %1, 0f00000000;" : "=f"(output) : "f"(product));
    return output;
}

#include "device-goose.h"

__kernel void goose_route_compact8(__global float *probabilities, __global int *ids,
                                  __global float *weights, float scale, ulong mailbox, uint sequence) {
    int lane;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    goose_compact_route_g((ulong)probabilities, (ulong)ids, (ulong)weights, scale, lane);
    __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory");
    if (lane == 0) {
        __asm__ volatile(
            "{ .reg .u32 a,b,c,d,e,f,g,h; "
            "ld.global.v4.u32 {a,b,c,d}, [%0]; "
            "ld.global.v4.u32 {e,f,g,h}, [%0+16]; "
            "st.global.wt.v4.u32 [%1], {a,b,c,d}; "
            "st.global.wt.v4.u32 [%1+16], {e,f,g,h}; "
            "st.release.sys.global.u32 [%1+32], %2; }"
            :: "l"((ulong)ids), "l"(mailbox), "r"(sequence) : "memory");
    }
}
