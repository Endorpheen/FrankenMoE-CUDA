// All four PQ2 codes are exact multiples of the stored F16 scale.
// Byte-permute directly into two F16 pairs, retaining signed zero.
static uint bonsai_pq2_pair(uint low, uint high, uint codes) {
    const uint selectors = 0x1010u | (codes&3u)*0x22u | ((codes>>2)&3u)*0x2200u;
    uint packed;
    __asm__("prmt.b32 %0, %1, %2, %3;"
            : "=r"(packed) : "r"(low), "r"(high), "r"(selectors));
    return packed;
}

static uint2 bonsai_pq2_quad(ushort scale, uint codes) {
    ushort twice;
    __asm__("add.rn.f16 %0, %1, %1;" : "=h"(twice) : "h"(scale));
    const uint low = (scale^0x8000u) | ((uint)(scale&0x8000u)<<16);
    const uint high = (uint)scale | ((uint)twice<<16);
    return (uint2)(bonsai_pq2_pair(low,high,codes), bonsai_pq2_pair(low,high,codes>>4));
}
