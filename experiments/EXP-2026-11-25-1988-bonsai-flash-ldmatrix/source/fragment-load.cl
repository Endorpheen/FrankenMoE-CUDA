static uint4 bonsai_load_a(__local const uint *base, uint stride, uint column, uint lane) {
    // Four row-block addresses: top-left, bottom-left, top-right, bottom-right.
    const uint address = (uint)(ulong)(base + (lane&15)*stride + column + (lane/16)*4);
    uint a0, a1, a2, a3;
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x4.shared.b16 {%0,%1,%2,%3}, [%4];"
                     : "=r"(a0), "=r"(a1), "=r"(a2), "=r"(a3) : "r"(address) : "memory");
    return (uint4)(a0,a1,a2,a3);
}

static uint2 bonsai_load_b(__local const uint *base, uint stride, uint column, uint lane) {
    // K and V staging both store each output column's K elements together.
    const uint address = (uint)(ulong)(base + (lane&7)*stride + column + ((lane/8)&1)*4);
    uint b0, b1;
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x2.shared.b16 {%0,%1}, [%2];"
                     : "=r"(b0), "=r"(b1) : "r"(address) : "memory");
    return (uint2)(b0,b1);
}

static void bonsai_copy16(__local uint *dst, __global const uint *src, uint available) {
    const uint address = (uint)(ulong)dst;
    __asm__ volatile("cp.async.ca.shared.global [%0], [%1], 16, %2;"
                     :: "r"(address), "l"((ulong)src), "r"(available) : "memory");
}

static void bonsai_copy_complete(void) {
    __asm__ volatile("cp.async.commit_group; cp.async.wait_group 0;" ::: "memory");
}
