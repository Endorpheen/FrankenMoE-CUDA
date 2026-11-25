static float bonsai_shuffle_up(float value, uint delta) {
    float peer;
    __asm__("shfl.sync.up.b32 %0, %1, %2, 0, -1;" : "=f"(peer) : "f"(value), "r"(delta));
    return peer;
}

static float bonsai_last_lane(float value) {
    float peer;
    __asm__("shfl.sync.idx.b32 %0, %1, 31, 31, -1;" : "=f"(peer) : "f"(value));
    return peer;
}

// Warp zero prepares the same keep/add coefficients as the scalar recurrence.
// Four warps consume them in increasing tile order with the original two
// multiplies and one addition per accumulator. Requires 128 threads.
__kernel void bonsai_flash_ordered_merge(__global const float *partial, __global float *output,
                                        uint queries, uint first_visible, uint tile_count, uint group_start) {
    __local float keep[32], add[32], inverse;
    const uint group = work_group()/12;
    const uint pair = group+group_start, r = work_group()%12, thread = local_id();
    const uint token = (pair/4)*2 + r/6, head = (pair%4)*6 + r%6;
    if (token >= queries) return;
    const uint rows = (first_visible+token+FLASH_KEYS-1)/FLASH_KEYS;
    const uint slot = (r/6)*8+r%6;
    const ulong stride = FLASH_ROWS*FLASH_PARTIAL;
    const __global float *part = partial + ((ulong)group*tile_count*FLASH_ROWS+slot)*FLASH_PARTIAL;
    float2 values = (float2)(0.0f);
    float maximum = as_float(0xff800000u);
    float sum = 0.0f;

    for (uint base = 0; base < rows; base += 32) {
        const uint active = flash_min(rows-base,32);
        if (thread < 32) {
            const float mx = thread < active ? part[(ulong)thread*stride] : as_float(0xff800000u);
            float seen = flash_max(maximum,mx);
            #pragma unroll
            for (uint bit = 1; bit < 32; bit *= 2) {
                const float prior = bonsai_shuffle_up(seen,bit);
                if (thread >= bit) seen = flash_max(prior,seen);
            }
            const float prior = bonsai_shuffle_up(seen,1);
            if (thread < active) {
                keep[thread] = flash_exp((thread == 0 ? maximum : prior)-seen);
                add[thread] = flash_exp(mx-seen);
            }
            maximum = bonsai_last_lane(seen);
        }
        sync_threads();
        for (uint i = 0; i < active; ++i) {
            const __global float *tile = part+(ulong)i*stride;
            const float2 input = *(__global const float2 *)(tile+2+2*thread);
            const float old = keep[i], weight = add[i];
            values = values*old + input*weight;
            if (thread == 0) sum = sum*old + tile[1]*weight;
        }
        part += 32*stride;
        sync_threads();
    }
    if (thread == 0) {
        float inv;
        __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inv) : "f"(sum));
        inverse = inv;
    }
    sync_threads();
    __global float2 *dst = (__global float2 *)(output+((ulong)token*24+head)*256);
    dst[thread] = values*inverse;
}
