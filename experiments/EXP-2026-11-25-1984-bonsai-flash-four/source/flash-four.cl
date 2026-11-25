#define FLASH_FOUR_ROWS 32
#define FLASH_FOUR_STRIDE 132

static uint flash_four_v_offset(uint feature, uint pair) {
    return feature*16 + (pair ^ (((feature/2)%4)*4));
}

// Two warp pairs evaluate four query positions. The score buffer is reused
// for the swizzled V fragment after all softmax rows have been packed.
__kernel void bonsai_flash_four(__global const float *query, __global const uint *keys,
                              __global const uint *values, __global float *partial,
                              uint queries, uint key_count, uint first_visible,
                              uint tile_count, uint group_start, uint chunks) {
    __local uint query_or_p[FLASH_FOUR_ROWS][FLASH_FOUR_STRIDE];
    __local uint k_tile[16][FLASH_FOUR_STRIDE];
    __local uint score_or_v[FLASH_FOUR_ROWS * FLASH_KEYS];
    __local float *score = (__local float *)score_or_v;

    const uint work = work_group() + group_start;
    const uint kv_head = work % 4, first = (work / 4) * 4;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + first + flash_min(queries-1-first,3)) return;
    const uint thread = local_id(), warp = thread/32, lane = thread%32;
    const uint lane_row = lane/4, lane_pair = lane%4, row = (warp/2)*16+lane_row;
    const uint warp_col = (warp%2)*8;
    __global float *out = partial + ((ulong)work_group()*tile_count + tile)*FLASH_FOUR_ROWS*FLASH_PARTIAL;

    for (uint i = thread; i < FLASH_FOUR_ROWS*128; i += 128) {
        const uint r = i/128, col = (i%128)*2;
        const uint token = first+r/8, head = r%8;
        uint packed = 0;
        if (token < queries && head < 6) {
            const __global float *q = query + ((ulong)token*24+kv_head*6+head)*256+col;
            packed = bonsai_half_pair(q[0],q[1]);
        }
        query_or_p[r][i%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 16) {
        const uint chunk = (key_start+kb)/32;
        const ulong base = ((ulong)kv_head*chunks+chunk)*4096+(kb%32)*128;
        for (uint i = thread; i < 16*128; i += 128) {
            k_tile[i/128][i%128] = chunk < chunks ? keys[base+i] : 0;
        }
        sync_threads();
        const uint key_row = warp_col+lane_row;
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint p = k+lane_pair;
            acc = bonsai_mma16x8(acc,query_or_p[row][p],query_or_p[row+8][p],
                query_or_p[row][p+4],query_or_p[row+8][p+4],k_tile[key_row][p],k_tile[key_row][p+4]);
        }
        const uint col = kb+warp_col+2*lane_pair;
        score[row*FLASH_KEYS+col] = acc.x*0.0625f;
        score[row*FLASH_KEYS+col+1] = acc.y*0.0625f;
        score[(row+8)*FLASH_KEYS+col] = acc.z*0.0625f;
        score[(row+8)*FLASH_KEYS+col+1] = acc.w*0.0625f;
        sync_threads();
    }

    const float neg_inf = as_float(0xff800000u);
    for (uint r = warp; r < FLASH_FOUR_ROWS; r += 4) {
        const uint token = first+r/8, visible = flash_min(key_count,first_visible+token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS,visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r*FLASH_KEYS+z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r*FLASH_KEYS+z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r*FLASH_KEYS+z+1]-mx) : 0.0f;
            sum += lo+hi;
            query_or_p[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[16];
    #pragma unroll
    for (uint n = 0; n < 16; ++n) acc[n] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        const uint chunk = (key_start+vb)/32;
        const ulong base = ((ulong)kv_head*chunks+chunk)*4096;
        for (uint i = thread; i < 4096; i += 128) {
            score_or_v[flash_four_v_offset(i/16,i%16)] = chunk < chunks ? values[base+i] : 0;
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint p = ksub+lane_pair, k = vb/2+p;
            const uint a0 = query_or_p[row][k], a1 = query_or_p[row+8][k];
            const uint a2 = query_or_p[row][k+4], a3 = query_or_p[row+8][k+4];
            #pragma unroll
            for (uint n = 0; n < 16; ++n) {
                const uint feature = 16*n+warp_col+lane_row;
                acc[n] = bonsai_mma16x8(acc[n],a0,a1,a2,a3,
                    score_or_v[flash_four_v_offset(feature,p)],score_or_v[flash_four_v_offset(feature,p+4)]);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint n = 0; n < 16; ++n) {
        const uint col = 16*n+warp_col+2*lane_pair+2;
        out[row*FLASH_PARTIAL+col] = acc[n].x;
        out[row*FLASH_PARTIAL+col+1] = acc[n].y;
        out[(row+8)*FLASH_PARTIAL+col] = acc[n].z;
        out[(row+8)*FLASH_PARTIAL+col+1] = acc[n].w;
    }
}
