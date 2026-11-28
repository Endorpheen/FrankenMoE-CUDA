#include "pipeline-mma-common.clh"
__kernel void bonsai_pq2_mmq_int8_full_n64(__global const uchar *weights, __global const uint *input,
                                 __global const float *scales, __global float *output,
                                 uint width,uint output_rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36],x[64][36];
    __local float weight_scale[64],token_scale[64];
    const uint rows=17408,blocks=40,row_base=64*work_group(),token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=32*(warp%2);
    const uint row=warp_row+lane/4, token=warp_token+2*(lane%4);
    float4 result[4];
    #pragma unroll
    for (uint i=0;i<4;++i) result[i]=(float4)(0.0f);

    for (uint block=0;block<blocks;++block) {
        for (uint packed=4*thread;packed<64*32;packed+=1024) {
            const uint t=token_base+packed/32;
            const ulong from=((ulong)t*blocks+block)*32+packed%32;
            bonsai_copy16(&x[packed/32][packed%32],input+from,16);
        }
        if (thread<64) {
            const uint token=token_base+thread;
            token_scale[thread]=scales[(ulong)token*blocks+block];
        }
        if (thread<64) {
            const uint row=row_base+thread;
            const __global ushort *encoded=(__global const ushort *)(weights+((ulong)row*blocks+block)*34);
            float scale;
            __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*encoded));
            weight_scale[thread]=scale;
        }
        for (uint i=thread;i<64*32;i+=256) {
            const uint r=row_base+i/32;
            const uint code=weights[((ulong)r*blocks+block)*34+2+i%32];
            w[i/32][i%32]=bonsai_expand_pq2_byte(code);
        }
        bonsai_copy_complete();
        sync_threads();

        int4 dot[4];
        #pragma unroll
        for (uint f=0;f<4;++f) dot[f]=(int4)(0);
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint f=0;f<4;++f) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*f][0],36,k,lane);
                dot[f]=bonsai_mma_i8(dot[f],a,b);
            }
        }
        const float wa=weight_scale[row],wb=weight_scale[row+8];
        #pragma unroll
        for (uint f=0;f<4;++f) {
            const float xa=token_scale[token+8*f],xb=token_scale[token+8*f+1];
            const float4 scale=(float4)(wa*xa,wa*xb,wb*xa,wb*xb);
            const int4 d=dot[f];
            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;
        }
        sync_threads();
    }
    #pragma unroll
    for (uint f=0;f<4;++f) {
        const ulong offset=(ulong)(token_base+token+8*f)*rows+row_base+row;
        output[offset]=result[f].x;
        output[offset+8]=result[f].z;
        output[offset+rows]=result[f].y;
        output[offset+rows+8]=result[f].w;
    }
}

__kernel void bonsai_pq2_mmq_int8_full_n128(__global const uchar *weights, __global const uint *input,
                                 __global const float *scales, __global float *output,
                                 uint width,uint output_rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36],x[128][36];
    __local float weight_scale[64],token_scale[128];
    const uint rows=17408,blocks=40,row_base=64*work_group(),token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint row=warp_row+lane/4, token=warp_token+2*(lane%4);
    float4 result[8];
    #pragma unroll
    for (uint i=0;i<8;++i) result[i]=(float4)(0.0f);

    for (uint block=0;block<blocks;++block) {
        for (uint packed=4*thread;packed<128*32;packed+=1024) {
            const uint t=token_base+packed/32;
            const ulong from=((ulong)t*blocks+block)*32+packed%32;
            bonsai_copy16(&x[packed/32][packed%32],input+from,16);
        }
        if (thread<128) {
            const uint token=token_base+thread;
            token_scale[thread]=scales[(ulong)token*blocks+block];
        }
        if (thread<64) {
            const uint row=row_base+thread;
            const __global ushort *encoded=(__global const ushort *)(weights+((ulong)row*blocks+block)*34);
            float scale;
            __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*encoded));
            weight_scale[thread]=scale;
        }
        for (uint i=thread;i<64*32;i+=256) {
            const uint r=row_base+i/32;
            const uint code=weights[((ulong)r*blocks+block)*34+2+i%32];
            w[i/32][i%32]=bonsai_expand_pq2_byte(code);
        }
        bonsai_copy_complete();
        sync_threads();

        int4 dot[8];
        #pragma unroll
        for (uint f=0;f<8;++f) dot[f]=(int4)(0);
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint f=0;f<8;++f) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*f][0],36,k,lane);
                dot[f]=bonsai_mma_i8(dot[f],a,b);
            }
        }
        const float wa=weight_scale[row],wb=weight_scale[row+8];
        #pragma unroll
        for (uint f=0;f<8;++f) {
            const float xa=token_scale[token+8*f],xb=token_scale[token+8*f+1];
            const float4 scale=(float4)(wa*xa,wa*xb,wb*xa,wb*xb);
            const int4 d=dot[f];
            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;
        }
        sync_threads();
    }
    #pragma unroll
    for (uint f=0;f<8;++f) {
        const ulong offset=(ulong)(token_base+token+8*f)*rows+row_base+row;
        output[offset]=result[f].x;
        output[offset+8]=result[f].z;
        output[offset+rows]=result[f].y;
        output[offset+rows+8]=result[f].w;
    }
}
__kernel void bonsai_pq2_mmq_int8_full_n128_pairload(__global const uchar *weights, __global const uint *input,
                                 __global const float *scales, __global float *output,
                                 uint width,uint output_rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36],x[128][36];
    __local float weight_scale[64],token_scale[128];
    const uint rows=17408,blocks=40,row_base=64*work_group(),token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint row=warp_row+lane/4, token=warp_token+2*(lane%4);
    float4 result[8];
    #pragma unroll
    for (uint i=0;i<8;++i) result[i]=(float4)(0.0f);

    for (uint block=0;block<blocks;++block) {
        for (uint packed=4*thread;packed<128*32;packed+=1024) {
            const uint t=token_base+packed/32;
            const ulong from=((ulong)t*blocks+block)*32+packed%32;
            bonsai_copy16(&x[packed/32][packed%32],input+from,16);
        }
        if (thread<128) {
            const uint token=token_base+thread;
            token_scale[thread]=scales[(ulong)token*blocks+block];
        }
        if (thread<64) {
            const uint row=row_base+thread;
            const __global ushort *encoded=(__global const ushort *)(weights+((ulong)row*blocks+block)*34);
            float scale;
            __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*encoded));
            weight_scale[thread]=scale;
        }
        for (uint i=2*thread;i<64*32;i+=512) {
            const uint r=row_base+i/32;
            const __global ushort *codes=(const __global ushort *)(
                weights+((ulong)r*blocks+block)*34+2+i%32);
            const uint pair=*codes;
            const uint2 unpacked=(uint2)(bonsai_expand_pq2_byte(pair&255u),
                                         bonsai_expand_pq2_byte(pair>>8));
            *(__local uint2 *)&w[i/32][i%32]=unpacked;
        }
        bonsai_copy_complete();
        sync_threads();

        int4 dot[8];
        #pragma unroll
        for (uint f=0;f<8;++f) dot[f]=(int4)(0);
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint f=0;f<8;++f) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*f][0],36,k,lane);
                dot[f]=bonsai_mma_i8(dot[f],a,b);
            }
        }
        const float wa=weight_scale[row],wb=weight_scale[row+8];
        #pragma unroll
        for (uint f=0;f<8;++f) {
            const float xa=token_scale[token+8*f],xb=token_scale[token+8*f+1];
            const float4 scale=(float4)(wa*xa,wa*xb,wb*xa,wb*xb);
            const int4 d=dot[f];
            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;
        }
        sync_threads();
    }
    #pragma unroll
    for (uint f=0;f<8;++f) {
        const ulong offset=(ulong)(token_base+token+8*f)*rows+row_base+row;
        output[offset]=result[f].x;
        output[offset+8]=result[f].z;
        output[offset+rows]=result[f].y;
        output[offset+rows+8]=result[f].w;
    }
}
__kernel void bonsai_pack_q8_panel128(__global const float *input, __global uint *packed,
                                 __global float *scales, uint groups) {
    const uint thread=local_id(), lane=thread%32;
    const uint row=4*work_group()+thread/32;
    if (row>=groups) return;
    const __global float *x=input+(ulong)row*128+4*lane;
    const float4 v=(float4)(x[0],x[1],x[2],x[3]);
    float largest=as_float(as_uint(v.x)&0x7fffffffu);
    largest=flash_max(largest,as_float(as_uint(v.y)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.z)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.w)&0x7fffffffu));
    for (uint bit=16;bit;bit/=2) largest=flash_max(largest,flash_shuffle(largest,bit));
    const float scale=largest/127.0f;
    const uint token=row/40, block=row%40;
    const uint panel_row=((token/128)*40+block)*128+token%128;
    packed[(ulong)panel_row*32+lane]=bonsai_quantize_q8_value(v.x,scale)
        | (bonsai_quantize_q8_value(v.y,scale)<<8)
        | (bonsai_quantize_q8_value(v.z,scale)<<16)
        | (bonsai_quantize_q8_value(v.w,scale)<<24);
    if (lane==0) scales[panel_row]=scale;
}
__kernel void bonsai_pq2_mmq_int8_full_n128_panel(__global const uchar *weights, __global const uint *input,
                                 __global const float *scales, __global float *output,
                                 uint width,uint output_rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36],x[128][36];
    __local float weight_scale[64],token_scale[128];
    const uint rows=17408,blocks=40,row_base=64*work_group(),panel=flash_grid_y(),token_base=128*panel;
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint row=warp_row+lane/4, token=warp_token+2*(lane%4);
    float4 result[8];
    #pragma unroll
    for (uint i=0;i<8;++i) result[i]=(float4)(0.0f);

    for (uint block=0;block<blocks;++block) {
        const uint panel_row=(panel*blocks+block)*128;
        for (uint packed=4*thread;packed<128*32;packed+=1024) {
            const ulong from=(ulong)panel_row*32+packed;
            bonsai_copy16(&x[packed/32][packed%32],input+from,16);
        }
        if (thread<128) {
            token_scale[thread]=scales[panel_row+thread];
        }
        if (thread<64) {
            const uint row=row_base+thread;
            const __global ushort *encoded=(__global const ushort *)(weights+((ulong)row*blocks+block)*34);
            float scale;
            __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*encoded));
            weight_scale[thread]=scale;
        }
        for (uint i=2*thread;i<64*32;i+=512) {
            const uint r=row_base+i/32;
            const __global ushort *codes=(const __global ushort *)(
                weights+((ulong)r*blocks+block)*34+2+i%32);
            const uint pair=*codes;
            const uint2 unpacked=(uint2)(bonsai_expand_pq2_byte(pair&255u),
                                         bonsai_expand_pq2_byte(pair>>8));
            *(__local uint2 *)&w[i/32][i%32]=unpacked;
        }
        bonsai_copy_complete();
        sync_threads();

        int4 dot[8];
        #pragma unroll
        for (uint f=0;f<8;++f) dot[f]=(int4)(0);
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint f=0;f<8;++f) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*f][0],36,k,lane);
                dot[f]=bonsai_mma_i8(dot[f],a,b);
            }
        }
        const float wa=weight_scale[row],wb=weight_scale[row+8];
        #pragma unroll
        for (uint f=0;f<8;++f) {
            const float xa=token_scale[token+8*f],xb=token_scale[token+8*f+1];
            const float4 scale=(float4)(wa*xa,wa*xb,wb*xa,wb*xb);
            const int4 d=dot[f];
            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;
        }
        sync_threads();
    }
    #pragma unroll
    for (uint f=0;f<8;++f) {
        const ulong offset=(ulong)(token_base+token+8*f)*rows+row_base+row;
        output[offset]=result[f].x;
        output[offset+8]=result[f].z;
        output[offset+rows]=result[f].y;
        output[offset+rows+8]=result[f].w;
    }
}
__kernel void bonsai_pq2_mmq_int8_full_n128_spread(__global const uchar *weights, __global const uint *input,
                                 __global const float *scales, __global float *output,
                                 uint width,uint output_rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36],x[128][36];
    __local float weight_scale[64],token_scale[128];
    const uint rows=17408,blocks=40,row_base=64*work_group(),panel=flash_grid_y(),token_base=128*panel;
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint row=warp_row+lane/4, token=warp_token+2*(lane%4);
    float4 result[8];
    #pragma unroll
    for (uint i=0;i<8;++i) result[i]=(float4)(0.0f);

    for (uint block=0;block<blocks;++block) {
        const uint panel_row=(panel*blocks+block)*128;
        for (uint packed=4*thread;packed<128*32;packed+=1024) {
            const ulong from=(ulong)panel_row*32+packed;
            bonsai_copy16(&x[packed/32][packed%32],input+from,16);
        }
        if (thread<128) {
            token_scale[thread]=scales[panel_row+thread];
        }
        if (thread<64) {
            const uint row=row_base+thread;
            const __global ushort *encoded=(__global const ushort *)(weights+((ulong)row*blocks+block)*34);
            float scale;
            __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*encoded));
            weight_scale[thread]=scale;
        }
        for (uint i=2*thread;i<64*32;i+=512) {
            const uint r=row_base+i/32;
            const __global ushort *codes=(const __global ushort *)(
                weights+((ulong)r*blocks+block)*34+2+i%32);
            const uint pair=*codes;
            uint selector=(pair|(pair<<8))&0x00ff00ffu;
            selector=(selector|(selector<<4))&0x0f0f0f0fu;
            selector=(selector|(selector<<2))&0x33333333u;
            uint2 unpacked;
            uint low,high;
            __asm__("prmt.b32 %0, %1, %1, %2;" : "=r"(low) : "r"(0x020100ffu), "r"(selector&65535u));
            __asm__("prmt.b32 %0, %1, %1, %2;" : "=r"(high) : "r"(0x020100ffu), "r"(selector>>16));
            unpacked=(uint2)(low,high);
            *(__local uint2 *)&w[i/32][i%32]=unpacked;
        }
        bonsai_copy_complete();
        sync_threads();

        int4 dot[8];
        #pragma unroll
        for (uint f=0;f<8;++f) dot[f]=(int4)(0);
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint f=0;f<8;++f) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*f][0],36,k,lane);
                dot[f]=bonsai_mma_i8(dot[f],a,b);
            }
        }
        const float wa=weight_scale[row],wb=weight_scale[row+8];
        #pragma unroll
        for (uint f=0;f<8;++f) {
            const float xa=token_scale[token+8*f],xb=token_scale[token+8*f+1];
            const float4 scale=(float4)(wa*xa,wa*xb,wb*xa,wb*xb);
            const int4 d=dot[f];
            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;
        }
        sync_threads();
    }
    #pragma unroll
    for (uint f=0;f<8;++f) {
        const ulong offset=(ulong)(token_base+token+8*f)*rows+row_base+row;
        output[offset]=result[f].x;
        output[offset+8]=result[f].z;
        output[offset+rows]=result[f].y;
        output[offset+rows+8]=result[f].w;
    }
}
__kernel void bonsai_decode_direct_q4_mma(__global const float *query, __global const uchar *keys,
                                   __global const uchar *values, __global float *partial,
                                   uint key_count, uint tile_count) {
    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local __attribute__((aligned(16))) uint raw[32 * 36];
    __local float score[6][256u];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][256u/2];

    const uint kv_head = work_group();
    const uint tile = flash_grid_y(), key_start = tile * 256u;
    const uint live = flash_min(256u,key_count-key_start);
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    const ulong head_stride = (ulong)tile_count*FLASH_PARTIAL;
    __global float *out = partial + kv_head*6*head_stride + tile*FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        uint packed = 0;
        if (r < 6) {
            const __global float *q = query + (kv_head*6+r)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < 256u; kb += 32) {
        flash_copy_q4_tile(raw, keys, kv_head, key_start+kb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            staging[key*132+pair] = key_start+kb+key < key_count
                ? local_flash_pack_q4_pair((__local const uchar *)(raw + key*36), pair) : 0;
        }
        sync_threads();
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
            const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
            acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        if (lane_row < 6) {
            score[lane_row][key_col] = acc.x * 0.0625f;
            score[lane_row][key_col+1] = acc.y * 0.0625f;
        }
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint count = r < 6 ? live : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < 256u; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0 && r < 6) {
            out[r*head_stride] = mx;
            out[r*head_stride+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < 256u; vb += 32) {
        flash_copy_q4_tile(raw, values, kv_head, key_start+vb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            const uint packed = key_start+vb+key < key_count
                ? local_flash_pack_q4_pair((__local const uchar *)(raw + key*36), pair) : 0;
            __local ushort *values_t = (__local ushort *)staging;
            values_t[pair*80+key] = (ushort)packed;
            values_t[pair*80+40+key] = (ushort)(packed>>16);
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint4 a = bonsai_load_a(&probability[0][0],256u/2,vb/2+ksub,lane);
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature_block = (32*result + 8*warp)*20;
                const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        if (lane_row < 6) {
            out[lane_row*head_stride+column] = acc[result].x;
            out[lane_row*head_stride+column+1] = acc[result].y;
        }
    }
}
static inline void verify_merge_store(__global float *dest, float2 value, float mass) {
    if (mass > 0) {
        float a, b;
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(a) : "f"(value.x), "f"(mass));
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(b) : "f"(value.y), "f"(mass));
        value = (float2)(a, b);
    } else {
        value = (float2)(0.0f);
    }
    *(__global float2 *)dest = value;
}

static inline __attribute__((always_inline)) void verify_merge_impl(
    __global const float *scratch, __global float *out, uint first_keys, uint max_tiles,
    uint parallel, __local float *work) {
    const uint job = work_group(), thread = local_id(), offset = thread * 2;
    const uint token = job / 24, head = job % 24;
    const uint tiles = (first_keys + token + 255) / 256;
    const __global float *part = scratch + ((size_t)token * 24 * max_tiles + head * tiles) * 258;
    __global float *dest = out + (size_t)job * 256 + offset;
    float2 values = (float2)(0.0f);
    float maximum = as_float(0xff800000u), mass = 0.0f;

    if (!parallel) {
        for (uint t = 0; t < tiles; ++t) {
            const __global float *tile = part + t * 258;
            const float m = tile[1];
            if (m == 0.0f) continue;
            const float mx = tile[0];
            const float next = mx > maximum ? mx : maximum;
            const float keep = attention_exp(maximum - next), add = attention_exp(mx - next);
            maximum = next;
            mass = mass * keep + m * add;
            values = values * keep + *(__global const float2 *)(tile + 2 + offset) * add;
        }
        verify_merge_store(dest, values, mass);
        return;
    }

    __local float *prev = work, *next_weight = work + 32, *tile_mass = work + 64;
    for (uint base = 0; base < tiles; base += 32) {
        const uint active = tiles - base < 32 ? tiles - base : 32;
        if (thread < 32) {
            const __global float *tile = part + (base + thread) * 258;
            const float m = thread < active ? tile[1] : 0.0f;
            const float mx = m == 0.0f ? as_float(0xff800000u) : tile[0];
            float seen = mx > maximum ? mx : maximum;
            #pragma unroll
            for (uint bit = 1; bit < 32; bit *= 2) {
                const float peer = bonsai_shuffle_up(seen, bit);
                if (thread >= bit) seen = seen > peer ? seen : peer;
            }
            const float before = bonsai_shuffle_up(seen, 1);
            tile_mass[thread] = m;
            if (m != 0.0f) {
                prev[thread] = attention_exp((thread == 0 ? maximum : before) - seen);
                next_weight[thread] = attention_exp(mx - seen);
            }
            maximum = bonsai_last_lane(seen);
        }
        sync_threads();
        for (uint i = 0; i < active; ++i) {
            const float m = tile_mass[i];
            if (m == 0.0f) continue;
            const float keep = prev[i], add = next_weight[i];
            const __global float *tile = part + (base + i) * 258;
            if (thread == 0) mass = mass * keep + m * add;
            values = values * keep + *(__global const float2 *)(tile + 2 + offset) * add;
        }
        sync_threads();
    }
    if (thread == 0) work[96] = mass;
    sync_threads();
    verify_merge_store(dest, values, work[96]);
}

__kernel void bonsai_verify_merge_direct(__global const float *scratch, __global float *out,
    uint first_keys, uint max_tiles) {
    verify_merge_impl(scratch, out, first_keys, max_tiles, 0, (__local float *)0);
}

__kernel void bonsai_verify_merge_parallel(__global const float *scratch, __global float *out,
    uint first_keys, uint max_tiles) {
    __local float work[97];
    verify_merge_impl(scratch, out, first_keys, max_tiles, 1, work);
}

static inline uint decode_pack_q4_half2(__local const uchar *row, uint pair) {
    const uint block=pair/16, index=pair%16;
    const __local ushort *data=(__local const ushort *)(row+18*block);
    const uint bits=data[0];
    const uint bytes=(uint)data[1+(index%8)];
    const uint q=(bytes>>(index>=8 ? 4 : 0))&0x0f0fu;
    const uint biased=0x64006400u|(q&15u)|((q&0x0f00u)<<8);
    const uint scale=bits|(bits<<16);
    uint values, result;
    __asm__("sub.rn.f16x2 %0, %1, %2;" : "=r"(values) : "r"(biased), "r"(0x64086408u));
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(result) : "r"(values), "r"(scale));
    return result;
}
__kernel void bonsai_decode_direct_q4_mma_half2(__global const float *query, __global const uchar *keys,
                                   __global const uchar *values, __global float *partial,
                                   uint key_count, uint tile_count) {
    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local __attribute__((aligned(16))) uint raw[32 * 36];
    __local float score[6][256u];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][256u/2];

    const uint kv_head = work_group();
    const uint tile = flash_grid_y(), key_start = tile * 256u;
    const uint live = flash_min(256u,key_count-key_start);
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    const ulong head_stride = (ulong)tile_count*FLASH_PARTIAL;
    __global float *out = partial + kv_head*6*head_stride + tile*FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        uint packed = 0;
        if (r < 6) {
            const __global float *q = query + (kv_head*6+r)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < 256u; kb += 32) {
        flash_copy_q4_tile(raw, keys, kv_head, key_start+kb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            staging[key*132+pair] = key_start+kb+key < key_count
                ? decode_pack_q4_half2((__local const uchar *)(raw + key*36), pair) : 0;
        }
        sync_threads();
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
            const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
            acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        if (lane_row < 6) {
            score[lane_row][key_col] = acc.x * 0.0625f;
            score[lane_row][key_col+1] = acc.y * 0.0625f;
        }
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint count = r < 6 ? live : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < 256u; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0 && r < 6) {
            out[r*head_stride] = mx;
            out[r*head_stride+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < 256u; vb += 32) {
        flash_copy_q4_tile(raw, values, kv_head, key_start+vb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            const uint packed = key_start+vb+key < key_count
                ? decode_pack_q4_half2((__local const uchar *)(raw + key*36), pair) : 0;
            __local ushort *values_t = (__local ushort *)staging;
            values_t[pair*80+key] = (ushort)packed;
            values_t[pair*80+40+key] = (ushort)(packed>>16);
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint4 a = bonsai_load_a(&probability[0][0],256u/2,vb/2+ksub,lane);
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature_block = (32*result + 8*warp)*20;
                const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        if (lane_row < 6) {
            out[lane_row*head_stride+column] = acc[result].x;
            out[lane_row*head_stride+column+1] = acc[result].y;
        }
    }
}
__kernel void bonsai_ffn_tail_q8_panel(__global const float *gate,__global const float *up,
                              __global const int *signs,__global uint *packed,__global float *scales,uint width) {
    __local float values[1024];
    const ulong token=(ulong)flash_grid_y()*width;
    const uint base=work_group()*1024,lane=local_id();
    for (uint i=lane;i<1024;i+=256) {
        const float result=bonsai_ffn_swiglu(gate[token+base+i],up[token+base+i]);
        values[i]=result*(float)signs[base+i];
    }
    sync_threads();
    for (uint span=1;span<1024;span*=2) {
        for (uint i=lane;i<512;i+=256) {
            const uint a=(i/span)*(2*span)+i%span;
            const float x=values[a],y=values[a+span];
            values[a]=x+y;
            values[a+span]=x-y;
        }
        sync_threads();
    }
    const uint warp=lane/32, in_warp=lane%32;
    const uint first=warp*128+in_warp*4;
    const float4 v=(float4)(values[first],values[first+1],values[first+2],values[first+3])*0.03125f;
    float largest=as_float(as_uint(v.x)&0x7fffffffu);
    largest=flash_max(largest,as_float(as_uint(v.y)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.z)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.w)&0x7fffffffu));
    for (uint bit=16;bit;bit/=2) largest=flash_max(largest,flash_shuffle(largest,bit));
    const float scale=largest/127.0f;
    const uint token_id=flash_grid_y(), block=work_group()*8+warp;
    const ulong row=((ulong)(token_id/128)*136+block)*128+token_id%128;
    packed[row*32+in_warp]=bonsai_quantize_q8_value(v.x,scale)
        | (bonsai_quantize_q8_value(v.y,scale)<<8)
        | (bonsai_quantize_q8_value(v.z,scale)<<16)
        | (bonsai_quantize_q8_value(v.w,scale)<<24);
    if (in_warp==0) scales[row]=scale;
}
__kernel void bonsai_pq2_mmq_int8_down_n128_spread(__global const uchar *weights, __global const uint *input,
                                 __global const float *scales, __global float *output,
                                 uint width,uint output_rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36],x[128][36];
    __local float weight_scale[64],token_scale[128];
    const uint rows=5120,blocks=136,row_base=64*work_group(),panel=flash_grid_y(),token_base=128*panel;
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint row=warp_row+lane/4, token=warp_token+2*(lane%4);
    float4 result[8];
    #pragma unroll
    for (uint i=0;i<8;++i) result[i]=(float4)(0.0f);

    for (uint block=0;block<blocks;++block) {
        const uint panel_row=(panel*blocks+block)*128;
        for (uint packed=4*thread;packed<128*32;packed+=1024) {
            const ulong from=(ulong)panel_row*32+packed;
            bonsai_copy16(&x[packed/32][packed%32],input+from,16);
        }
        if (thread<128) {
            token_scale[thread]=scales[panel_row+thread];
        }
        if (thread<64) {
            const uint row=row_base+thread;
            const __global ushort *encoded=(__global const ushort *)(weights+((ulong)row*blocks+block)*34);
            float scale;
            __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*encoded));
            weight_scale[thread]=scale;
        }
        for (uint i=2*thread;i<64*32;i+=512) {
            const uint r=row_base+i/32;
            const __global ushort *codes=(const __global ushort *)(
                weights+((ulong)r*blocks+block)*34+2+i%32);
            const uint pair=*codes;
            uint selector=(pair|(pair<<8))&0x00ff00ffu;
            selector=(selector|(selector<<4))&0x0f0f0f0fu;
            selector=(selector|(selector<<2))&0x33333333u;
            uint2 unpacked;
            uint low,high;
            __asm__("prmt.b32 %0, %1, %1, %2;" : "=r"(low) : "r"(0x020100ffu), "r"(selector&65535u));
            __asm__("prmt.b32 %0, %1, %1, %2;" : "=r"(high) : "r"(0x020100ffu), "r"(selector>>16));
            unpacked=(uint2)(low,high);
            *(__local uint2 *)&w[i/32][i%32]=unpacked;
        }
        bonsai_copy_complete();
        sync_threads();

        int4 dot[8];
        #pragma unroll
        for (uint f=0;f<8;++f) dot[f]=(int4)(0);
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint f=0;f<8;++f) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*f][0],36,k,lane);
                dot[f]=bonsai_mma_i8(dot[f],a,b);
            }
        }
        const float wa=weight_scale[row],wb=weight_scale[row+8];
        #pragma unroll
        for (uint f=0;f<8;++f) {
            const float xa=token_scale[token+8*f],xb=token_scale[token+8*f+1];
            const float4 scale=(float4)(wa*xa,wa*xb,wb*xa,wb*xb);
            const int4 d=dot[f];
            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;
        }
        sync_threads();
    }
    #pragma unroll
    for (uint f=0;f<8;++f) {
        const ulong offset=(ulong)(token_base+token+8*f)*rows+row_base+row;
        output[offset]=result[f].x;
        output[offset+8]=result[f].z;
        output[offset+rows]=result[f].y;
        output[offset+rows+8]=result[f].w;
    }
}
typedef struct {
    uint token;
    float logit;
    uint invalid;
} bonsai_choice;

static inline bonsai_choice choice_empty(void) {
    return (bonsai_choice){0xffffffffu, as_float(0xff800000u), 0};
}

static inline bonsai_choice choice_best(bonsai_choice a, bonsai_choice b) {
    const uint invalid = a.invalid | b.invalid;
    if (b.logit > a.logit || (b.logit == a.logit && b.token < a.token)) a = b;
    a.invalid = invalid;
    return a;
}

static inline uint choice_peer(uint value, uint distance) {
    uint peer;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=r"(peer) : "r"(value), "r"(distance));
    return peer;
}

static inline bonsai_choice choice_warp(bonsai_choice value) {
    for (uint distance = 16; distance; distance >>= 1) {
        const bonsai_choice peer = {
            choice_peer(value.token, distance),
            as_float(choice_peer(as_uint(value.logit), distance)),
            choice_peer(value.invalid, distance),
        };
        value = choice_best(value, peer);
    }
    return value;
}

static inline bonsai_choice choice_block(bonsai_choice value, __local bonsai_choice *scratch) {
    const uint thread = local_id();
    value = choice_warp(value);
    if ((thread & 31) == 0) scratch[thread / 32] = value;
    sync_threads();
    if (thread < 32) value = choice_warp(thread < 8 ? scratch[thread] : choice_empty());
    return value;
}

__kernel void bonsai_choice_tiles(__global const float *logits,
    __global const uint *mask, __global bonsai_choice *tiles, uint width, uint jobs) {
    const uint block = work_group(), row = bonsai_grid_y();
    __local bonsai_choice scratch[8];
    bonsai_choice maximum = choice_empty();
    for (uint i = 0; i < 4; ++i) {
        const uint token = block * 1024 + i * 256 + local_id();
        if (token >= width) continue;
        const float x = logits[(size_t)row * width + token];
        if ((as_uint(x) & 0x7f800000u) == 0x7f800000u) {
            maximum.invalid = 1;
        } else if (!mask || ((mask[token / 32] >> (token % 32)) & 1u) == 0) {
            maximum = choice_best(maximum, (bonsai_choice){token, x, 0});
        }
    }
    maximum = choice_block(maximum, scratch);
    if (local_id() == 0) tiles[(size_t)row * jobs + block] = maximum;
}

__kernel void bonsai_choice_final(__global const bonsai_choice *tiles,
    __global bonsai_choice *result, uint jobs) {
    const uint row = work_group(), thread = local_id();
    __local bonsai_choice scratch[8];
    const bonsai_choice value = thread < jobs ? tiles[(size_t)row * jobs + thread] : choice_empty();
    const bonsai_choice best = choice_block(value, scratch);
    if (thread == 0) result[row] = best;
}
