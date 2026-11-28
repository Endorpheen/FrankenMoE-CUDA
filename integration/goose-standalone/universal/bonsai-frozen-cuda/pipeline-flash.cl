#include "pipeline-common.clh"

static uint bonsai_fp8_four(float4 value) {
    ushort low, high;
    __asm__("cvt.rn.satfinite.e4m3x2.f32 %0, %2, %1;" : "=h"(low) : "f"(value.x), "f"(value.y));
    __asm__("cvt.rn.satfinite.e4m3x2.f32 %0, %2, %1;" : "=h"(high) : "f"(value.z), "f"(value.w));
    return (uint)low | ((uint)high<<16);
}

static float4 bonsai_mma_fp8(float4 acc, uint4 a, uint2 b) {
    float x=acc.x,y=acc.y,z=acc.z,w=acc.w;
    __asm__("mma.sync.aligned.m16n8k32.row.col.f32.e4m3.e4m3.f32 "
            "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
            : "+f"(x),"+f"(y),"+f"(z),"+f"(w)
            : "r"(a.x),"r"(a.y),"r"(a.z),"r"(a.w),"r"(b.x),"r"(b.y));
    return (float4)(x,y,z,w);
}

__kernel void bonsai_flash_stage_k_fp8(__global const uchar *k,
                                      __global uint *packed_k,
                                      uint keys,uint chunks) {
    const uint block=work_group(),head=flash_grid_y(),thread=local_id();
    const ulong base=((ulong)head*chunks+block)*2048;
    for (uint i=thread;i<2048;i+=128) {
        const uint key=block*32+i/64,quad=i%64;
        uint packed=0;
        if (key<keys) {
            const __global uchar *row=k+(ulong)key*576+head*144;
            const float2 a=flash_q4_pair(row,quad*2),b=flash_q4_pair(row,quad*2+1);
            packed=bonsai_fp8_four((float4)(a.x,a.y,b.x,b.y));
        }
        packed_k[base+i]=packed;
    }
}

__kernel void bonsai_flash_stage_v_only(__global const uchar *v,
                                       __global uint *packed_v,
                                       uint keys,uint chunks) {
    __local uint tile[32][132];
    const uint block=work_group(),head=flash_grid_y(),thread=local_id();
    const ulong base=((ulong)head*chunks+block)*4096;
    for (uint i=thread;i<4096;i+=128) {
        const uint key=block*32+i/128,pair=i%128;
        tile[i/128][pair]=key<keys ? flash_pack_q4_pair(v+(ulong)key*576+head*144,pair) : 0;
    }
    sync_threads();
    for (uint i=thread;i<4096;i+=128) {
        const uint feature=i/16,key=2*(i%16),shift=16*(feature%2);
        const uint lo=(tile[key][feature/2]>>shift)&0xffff;
        const uint hi=(tile[key+1][feature/2]>>shift)&0xffff;
        packed_v[base+i]=lo|(hi<<16);
    }
}

__kernel void bonsai_flash_fp8_qk(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *output,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint panel_scratch[10368];
    const uint physical_thread = local_id();
    const uint panel = physical_thread / 128;
    __local uint (*query_fp8)[68] = (__local uint (*)[68])(panel_scratch + panel * 1088);
    __local uint (*score)[FLASH_KEYS] = (__local uint (*)[FLASH_KEYS])(panel_scratch + 2176 + panel * 2048);
    __local uint (*probability)[FLASH_KEYS] = score;
    __local uint *staging = panel_scratch + 6272;

    const uint group = work_group() * 2 + panel;
    const uint work = group + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint pair_last = flash_min(first_row+(2-panel)*FLASH_ROWS, queries*6)-1;

    const uint thread = physical_thread % 128, warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);


    for (uint index = thread; index < FLASH_ROWS * 64; index += 128) {
        const uint r = index / 64, col = (index % 64)*4;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_fp8_four((float4)(q[0],q[1],q[2],q[3]));
        }
        query_fp8[r][index%64] = packed;
    }
    if (thread < FLASH_ROWS) {
        query_fp8[thread][64] = as_uint(neg_inf);
        query_fp8[thread][65] = 0;
    }
    sync_threads();

    float4 values_acc[8];
    #pragma unroll
    for (uint r = 0; r < 8; ++r) values_acc[r] = (float4)(0.0f);
    const uint visible_end = flash_min(key_count, first_visible + pair_last/6);
    for (uint tile = 0; tile < tile_count; ++tile) {
        const uint key_start = tile * FLASH_KEYS;
        if (key_start >= visible_end) break;
        for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
            const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*2048 : 0;
            for (uint index = physical_thread*4; index < 32*64; index += 1024) {
                bonsai_copy16(staging+(index/64)*68+index%64,keys+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            float4 acc = (float4)(0.0f);
            #pragma unroll 8
            for (uint k = 0; k < 64; k += 8) {
                const uint4 a = bonsai_load_a(&query_fp8[0][0],68,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*68,68,k,lane);
                acc = bonsai_mma_fp8(acc,a,b);
            }
            const uint key_col = kb + warp*8 + 2*lane_pair;
            score[lane_row][key_col] = as_uint(acc.x * 0.0625f);
            score[lane_row][key_col+1] = as_uint(acc.y * 0.0625f);
            score[lane_row+8][key_col] = as_uint(acc.z * 0.0625f);
            score[lane_row+8][key_col+1] = as_uint(acc.w * 0.0625f);
            sync_threads();
        }

        for (uint r = warp; r < FLASH_ROWS; r += 4) {
            const uint token = (first_row+r)/6;
            const uint visible = flash_min(key_count, first_visible + token);
            const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
            float mx = neg_inf;
            for (uint z = lane; z < count; z += 32) mx = flash_max(mx,as_float(score[r][z]));
            for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
            float sum = 0.0f;
            uint probabilities[2];
            for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
                const float lo = z < count ? flash_exp(as_float(score[r][z])-mx) : 0.0f;
                const float hi = z+1 < count ? flash_exp(as_float(score[r][z+1])-mx) : 0.0f;
                sum += lo + hi;
                probabilities[z/64] = bonsai_half_pair(lo,hi);
            }
            for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
            __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory");
            probability[r][lane] = probabilities[0];
            probability[r][lane+32] = probabilities[1];
            if (lane == 0) {
                if (count != 0) {
                    const float prior = as_float(query_fp8[r][64]), seen = flash_max(prior,mx);
                    const float keep = flash_exp(prior-seen);
                    const float add = flash_exp(mx-seen);
                    query_fp8[r][64] = as_uint(seen);
                    query_fp8[r][65] = as_uint(as_float(query_fp8[r][65])*keep + sum*add);
                    query_fp8[r][66] = as_uint(keep);
                    query_fp8[r][67] = as_uint(add);
                }
            }
        }
        sync_threads();

        float4 acc[8];
        #pragma unroll
        for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/16)*16+index%16,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            #pragma unroll
            for (uint ksub = 0; ksub < 16; ksub += 8) {
                const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS,vb/2+ksub,lane);
                #pragma unroll
                for (uint result = 0; result < 8; ++result) {
                    const uint feature_block = (32*result + 8*warp)*16;
                    const uint2 b = bonsai_load_b(staging+feature_block,16,ksub,lane);
                    acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
                }
            }
            sync_threads();
        }
        const uint token0 = (first_row+lane_row)/6, token1 = (first_row+lane_row+8)/6;
        const int valid0 = token0 < queries && first_visible+token0 > key_start;
        const int valid1 = token1 < queries && first_visible+token1 > key_start;
        #pragma unroll
        for (uint result = 0; result < 8; ++result) {
            if (valid0) {
                values_acc[result].xy = values_acc[result].xy*as_float(query_fp8[lane_row][66]) + acc[result].xy*as_float(query_fp8[lane_row][67]);
            }
            if (valid1) {
                values_acc[result].zw = values_acc[result].zw*as_float(query_fp8[lane_row+8][66]) + acc[result].zw*as_float(query_fp8[lane_row+8][67]);
            }
        }
        sync_threads();
    }

    if (thread < FLASH_ROWS) {
        float inv;
        __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inv) : "f"(as_float(query_fp8[thread][65])));
        query_fp8[thread][65] = as_uint(inv);
    }
    sync_threads();
    const uint row0 = first_row+lane_row, row1 = row0+8;
    const ulong offset0 = ((ulong)(row0/6)*24 + kv_head*6 + row0%6)*256;
    const ulong offset1 = ((ulong)(row1/6)*24 + kv_head*6 + row1%6)*256;
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint col = 32*result + 8*warp + 2*lane_pair;
        if (row0/6 < queries)
            *(__global float2 *)(output+offset0+col) = values_acc[result].xy*as_float(query_fp8[lane_row][65]);
        if (row1/6 < queries)
            *(__global float2 *)(output+offset1+col) = values_acc[result].zw*as_float(query_fp8[lane_row+8][65]);
    }
}


static float bonsai_fp8_v_feature(__global const uchar *v,uint key,uint keys,uint head,uint feature) {
    if (key>=keys) return 0.0f;
    const float2 pair=flash_q4_pair(v+(ulong)key*576+head*144,feature/2);
    return feature&1 ? pair.y : pair.x;
}

__kernel void bonsai_flash_stage_v_fp8(__global const uchar *v,
                                      __global uint *packed_v,
                                      uint keys,uint chunks) {
    const uint block=work_group(),head=flash_grid_y(),thread=local_id();
    const ulong base=((ulong)head*chunks+block)*2048;
    for (uint i=thread;i<2048;i+=128) {
        const uint feature=i/8,key=block*32+4*(i%8);
        const float4 value=(float4)(bonsai_fp8_v_feature(v,key,keys,head,feature),
            bonsai_fp8_v_feature(v,key+1,keys,head,feature),
            bonsai_fp8_v_feature(v,key+2,keys,head,feature),
            bonsai_fp8_v_feature(v,key+3,keys,head,feature));
        packed_v[base+i]=bonsai_fp8_four(value);
    }
}

__kernel void bonsai_flash_fp8_qkpv(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *output,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint panel_scratch[10368];
    const uint physical_thread = local_id();
    const uint panel = physical_thread / 128;
    __local uint (*query_fp8)[68] = (__local uint (*)[68])(panel_scratch + panel * 1088);
    __local uint (*score)[FLASH_KEYS] = (__local uint (*)[FLASH_KEYS])(panel_scratch + 2176 + panel * 2048);
    __local uint (*probability)[FLASH_KEYS] = score;
    __local uint *staging = panel_scratch + 6272;

    const uint group = work_group() * 2 + panel;
    const uint work = group + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint pair_last = flash_min(first_row+(2-panel)*FLASH_ROWS, queries*6)-1;

    const uint thread = physical_thread % 128, warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);


    for (uint index = thread; index < FLASH_ROWS * 64; index += 128) {
        const uint r = index / 64, col = (index % 64)*4;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_fp8_four((float4)(q[0],q[1],q[2],q[3]));
        }
        query_fp8[r][index%64] = packed;
    }
    if (thread < FLASH_ROWS) {
        query_fp8[thread][64] = as_uint(neg_inf);
        query_fp8[thread][65] = 0;
    }
    sync_threads();

    float4 values_acc[8];
    #pragma unroll
    for (uint r = 0; r < 8; ++r) values_acc[r] = (float4)(0.0f);
    const uint visible_end = flash_min(key_count, first_visible + pair_last/6);
    for (uint tile = 0; tile < tile_count; ++tile) {
        const uint key_start = tile * FLASH_KEYS;
        if (key_start >= visible_end) break;
        for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
            const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*2048 : 0;
            for (uint index = physical_thread*4; index < 32*64; index += 1024) {
                bonsai_copy16(staging+(index/64)*68+index%64,keys+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            float4 acc = (float4)(0.0f);
            #pragma unroll 8
            for (uint k = 0; k < 64; k += 8) {
                const uint4 a = bonsai_load_a(&query_fp8[0][0],68,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*68,68,k,lane);
                acc = bonsai_mma_fp8(acc,a,b);
            }
            const uint key_col = kb + warp*8 + 2*lane_pair;
            score[lane_row][key_col] = as_uint(acc.x * 0.0625f);
            score[lane_row][key_col+1] = as_uint(acc.y * 0.0625f);
            score[lane_row+8][key_col] = as_uint(acc.z * 0.0625f);
            score[lane_row+8][key_col+1] = as_uint(acc.w * 0.0625f);
            sync_threads();
        }

        for (uint r = warp; r < FLASH_ROWS; r += 4) {
            const uint token = (first_row+r)/6;
            const uint visible = flash_min(key_count, first_visible + token);
            const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
            float mx = neg_inf;
            for (uint z = lane; z < count; z += 32) mx = flash_max(mx,as_float(score[r][z]));
            for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
            float sum = 0.0f;
            ushort probabilities[2];
            for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
                const float lo = z < count ? flash_exp(as_float(score[r][z])-mx) : 0.0f;
                const float hi = z+1 < count ? flash_exp(as_float(score[r][z+1])-mx) : 0.0f;
                sum += lo + hi;
                ushort encoded;
                __asm__("cvt.rn.satfinite.e4m3x2.f32 %0,%2,%1;"
                    : "=h"(encoded) : "f"(lo*256.0f), "f"(hi*256.0f));
                probabilities[z/64] = encoded;
            }
            for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
            __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory");
            __local ushort *packed = (__local ushort *)probability[r];
            packed[lane] = probabilities[0];
            packed[lane+32] = probabilities[1];
            if (lane == 0) {
                if (count != 0) {
                    const float prior = as_float(query_fp8[r][64]), seen = flash_max(prior,mx);
                    const float keep = flash_exp(prior-seen);
                    const float add = flash_exp(mx-seen);
                    query_fp8[r][64] = as_uint(seen);
                    query_fp8[r][65] = as_uint(as_float(query_fp8[r][65])*keep + sum*add);
                    query_fp8[r][66] = as_uint(keep);
                    query_fp8[r][67] = as_uint(add);
                }
            }
        }
        sync_threads();

        float4 acc[8];
        #pragma unroll
        for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*2048 : 0;
            for (uint index = physical_thread*4; index < 2048; index += 1024) {
                bonsai_copy16(staging+(index/8)*12+index%8,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS,vb/4,lane);
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature_block = (32*result+8*warp)*12;
                const uint2 b = bonsai_load_b(staging+feature_block,12,0,lane);
                acc[result]=bonsai_mma_fp8(acc[result],a,b);
            }
            sync_threads();
        }
        const uint token0 = (first_row+lane_row)/6, token1 = (first_row+lane_row+8)/6;
        const int valid0 = token0 < queries && first_visible+token0 > key_start;
        const int valid1 = token1 < queries && first_visible+token1 > key_start;
        #pragma unroll
        for (uint result = 0; result < 8; ++result) {
            if (valid0) {
                values_acc[result].xy = values_acc[result].xy*as_float(query_fp8[lane_row][66]) + (acc[result].xy*(1.0f/256.0f))*as_float(query_fp8[lane_row][67]);
            }
            if (valid1) {
                values_acc[result].zw = values_acc[result].zw*as_float(query_fp8[lane_row+8][66]) + (acc[result].zw*(1.0f/256.0f))*as_float(query_fp8[lane_row+8][67]);
            }
        }
        sync_threads();
    }

    if (thread < FLASH_ROWS) {
        float inv;
        __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inv) : "f"(as_float(query_fp8[thread][65])));
        query_fp8[thread][65] = as_uint(inv);
    }
    sync_threads();
    const uint row0 = first_row+lane_row, row1 = row0+8;
    const ulong offset0 = ((ulong)(row0/6)*24 + kv_head*6 + row0%6)*256;
    const ulong offset1 = ((ulong)(row1/6)*24 + kv_head*6 + row1%6)*256;
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint col = 32*result + 8*warp + 2*lane_pair;
        if (row0/6 < queries)
            *(__global float2 *)(output+offset0+col) = values_acc[result].xy*as_float(query_fp8[lane_row][65]);
        if (row1/6 < queries)
            *(__global float2 *)(output+offset1+col) = values_acc[result].zw*as_float(query_fp8[lane_row+8][65]);
    }
}
