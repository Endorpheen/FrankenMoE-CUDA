static inline uint bonsai_pq2_permute(uint a, uint b, uint selector) {
    uint result;
    __asm__("prmt.b32 %0, %1, %2, %3;" : "=r"(result) : "r"(a), "r"(b), "r"(selector));
    return result;
}

static inline uint2 bonsai_pq2_half2(ushort bits, uint code) {
    const uint selector = (code & 3u) | ((code & 12u) << 2) |
        ((code & 48u) << 4) | ((code & 192u) << 6);
    const uint factors = bonsai_pq2_permute(0x403c00bcu, 0, selector);
    const uint a = bonsai_pq2_permute(factors, 0, 0x1404);
    const uint b = bonsai_pq2_permute(factors, 0, 0x3424);
    const uint scale = (uint)bits | ((uint)bits << 16);
    uint first, second;
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(first) : "r"(a), "r"(scale));
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(second) : "r"(b), "r"(scale));
    return (uint2)(first, second);
}

__kernel void bonsai_pq2_validate_unpack(__global uint *mismatches) {
    const uint id = work_group() * 256 + local_id();
    const ushort bits = (ushort)(id >> 8);
    const uint code = id & 255;
    if ((bits & 0x7c00u) == 0x7c00u) return;
    const uint2 expected = bonsai_ffn_unpack_cached(bits, code);
    const uint2 actual = bonsai_pq2_half2(bits, code);
    if (expected.x != actual.x || expected.y != actual.y)
        __asm__ volatile("red.global.add.u32 [%0], 1;" :: "l"((ulong)mismatches) : "memory");
}
