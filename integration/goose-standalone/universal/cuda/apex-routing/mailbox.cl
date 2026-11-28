#include "route-device-common.h"
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
