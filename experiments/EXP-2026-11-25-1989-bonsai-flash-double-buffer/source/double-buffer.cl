// Four consecutive uints are copied together. Shift each row by an
// integer number of 16-byte groups to preserve ldmatrix alignment.
static uint bonsai_swizzled_row(uint row, uint packed_col) {
    return row*128 + (packed_col ^ ((row&7)*4));
}

static uint4 bonsai_swizzled_a(__local const uint *base, uint column, uint lane) {
    const uint row = lane&15, col = column+(lane/16)*4;
    const uint address = (uint)(ulong)(base + bonsai_swizzled_row(row,col));
    uint a0,a1,a2,a3;
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x4.shared.b16 {%0,%1,%2,%3}, [%4];"
        : "=r"(a0),"=r"(a1),"=r"(a2),"=r"(a3) : "r"(address) : "memory");
    return (uint4)(a0,a1,a2,a3);
}

static uint2 bonsai_swizzled_b(__local const uint *base, uint first_row, uint column, uint lane) {
    const uint row = first_row+(lane&7), col = column+((lane/8)&1)*4;
    const uint address = (uint)(ulong)(base + bonsai_swizzled_row(row,col));
    uint b0,b1;
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x2.shared.b16 {%0,%1}, [%2];"
        : "=r"(b0),"=r"(b1) : "r"(address) : "memory");
    return (uint2)(b0,b1);
}

static void bonsai_stage_next(__local uint *dest, __global const uint *src,
                              uint chunk, uint chunks, uint head, uint thread, uint is_value) {
    const uint available = chunk < chunks ? 16 : 0;
    const ulong base = available ? ((ulong)head*chunks+chunk)*4096 : 0;
    for (uint i=thread*4; i<4096; i+=512) {
        const uint offset = is_value ? (i/16)*20+i%16 : bonsai_swizzled_row(i/128,i%128);
        bonsai_copy16(dest+offset,src+base+i,available);
    }
    __asm__ volatile("cp.async.commit_group;" ::: "memory");
}

static void bonsai_stage_next_l2(__local uint *dest, __global const uint *src,
                                 uint chunk, uint chunks, uint head, uint thread, uint is_value) {
    const uint available = chunk < chunks ? 16 : 0;
    const ulong base = available ? ((ulong)head*chunks+chunk)*4096 : 0;
    for (uint i=thread*4; i<4096; i+=512) {
        const uint offset = is_value ? (i/16)*20+i%16 : bonsai_swizzled_row(i/128,i%128);
        const uint address = (uint)(ulong)(dest+offset);
        __asm__ volatile("cp.async.cg.shared.global.L2::128B [%0], [%1], 16, %2;"
            :: "r"(address), "l"((ulong)(src+base+i)), "r"(available) : "memory");
    }
    __asm__ volatile("cp.async.commit_group;" ::: "memory");
}
