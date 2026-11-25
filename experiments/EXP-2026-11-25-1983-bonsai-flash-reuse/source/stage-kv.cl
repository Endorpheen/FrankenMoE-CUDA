// Expand each Q4 block once for all subsequent query groups. K is key-major,
// and each 32-key tile of V is feature-major in packed half pairs.
__kernel void bonsai_flash_stage_kv(__global const uchar *k, __global const uchar *v,
                                    __global uint *packed_k, __global uint *packed_v,
                                    uint keys, uint chunks) {
    __local uint tile[32][132];
    const uint block = work_group(), head = flash_grid_y();
    const uint thread = local_id();
    const ulong base = ((ulong)head * chunks + block) * 4096;
    for (uint i = thread; i < 4096; i += 128) {
        const uint key = block*32 + i/128, pair = i%128;
        uint kk = 0, vv = 0;
        if (key < keys) {
            const ulong at = (ulong)key*576 + head*144;
            kk = flash_pack_q4_pair(k+at,pair);
            vv = flash_pack_q4_pair(v+at,pair);
        }
        packed_k[base+i] = kk;
        tile[i/128][pair] = vv;
    }
    sync_threads();
    for (uint i = thread; i < 4096; i += 128) {
        const uint feature = i/16, key = 2*(i%16), s = 16*(feature%2);
        const uint lo = (tile[key][feature/2] >> s) & 0xffff;
        const uint hi = (tile[key+1][feature/2] >> s) & 0xffff;
        packed_v[base+i] = lo | (hi<<16);
    }
}
