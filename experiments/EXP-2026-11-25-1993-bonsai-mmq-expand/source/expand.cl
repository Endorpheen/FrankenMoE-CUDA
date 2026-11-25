// Expand one matrix into scratch. A thread writes the four half values
// represented by a byte. Width is a multiple of the PQ2 block size.
__kernel void bonsai_pq2_expand_half(__global const uchar *weights,
                                     __global uint *expanded, uint width, uint rows) {
    const uint work = work_group()*256 + local_id();
    const uint count = rows*(width/4);
    if (work >= count) return;
    const uint block = work/32, quad = work%32;
    const __global uchar *q = weights + (ulong)block*34;
    const uint2 half_values = bonsai_pq2_quad(*(__global const ushort *)q,q[2+quad]);
    expanded[2*(ulong)work] = half_values.x;
    expanded[2*(ulong)work+1] = half_values.y;
}

// Transpose the PQ2 blocks by K block and matrix row. The codes remain two
// bits each. Every code row becomes naturally aligned for a wide load.
__kernel void bonsai_pq2_split_planes(__global const uchar *weights, __global uint *codes,
                                      __global ushort *scales, uint blocks, uint rows) {
    const uint work = work_group()*256+local_id();
    const uint index = work/8, part = work%8;
    if (index >= blocks*rows) return;
    const uint row = index/blocks, block = index%blocks, target = block*rows+row;
    const __global ushort *packed = (__global const ushort *)(weights+(ulong)index*34);
    const uint lo = packed[1+part*2], hi = packed[2+part*2];
    codes[(ulong)target*8+part] = lo | (hi<<16);
    if (part==0) scales[target] = packed[0];
}
