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

#include "warp-shuffle.h"
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
