#include "route-device-common.h"
#include "device-goose.h"

__kernel void goose_route_compact8(__global float *probabilities, __global int *ids,
                                  __global float *weights, float scale) {
    int lane;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    goose_compact_route_g((ulong)probabilities, (ulong)ids, (ulong)weights, scale, lane);
}
