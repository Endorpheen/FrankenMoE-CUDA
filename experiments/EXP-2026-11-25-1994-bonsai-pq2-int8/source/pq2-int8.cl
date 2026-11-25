static uint bonsai_quantize_q8_value(float value, float scale) {
    int result = 0;
    if (scale > 0.0f) {
        const float normalized = value/scale;
        __asm__("cvt.rni.s32.f32 %0, %1;" : "=r"(result) : "f"(normalized));
        result = result < -127 ? -127 : (result > 127 ? 127 : result);
    }
    return (uint)result&255u;
}

// Launch 128 threads: each of the four warps owns one token/PQ2 block.
__kernel void bonsai_pack_q8_128(__global const float *input, __global uint *packed,
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
    packed[(ulong)row*32+lane]=bonsai_quantize_q8_value(v.x,scale)
        | (bonsai_quantize_q8_value(v.y,scale)<<8)
        | (bonsai_quantize_q8_value(v.z,scale)<<16)
        | (bonsai_quantize_q8_value(v.w,scale)<<24);
    if (lane==0) scales[row]=scale;
}

static uint bonsai_expand_pq2_byte(uint code) {
    const uint selector=(code&3u) | ((code&12u)<<2) | ((code&48u)<<4) | ((code&192u)<<6);
    uint result;
    __asm__("prmt.b32 %0, %1, %1, %2;" : "=r"(result) : "r"(0x020100ffu), "r"(selector));
    return result;
}

static int4 bonsai_mma_i8(int4 sum, uint4 a, uint2 b) {
    int s0=sum.x,s1=sum.y,s2=sum.z,s3=sum.w;
    __asm__("mma.sync.aligned.m16n8k32.row.col.s32.s8.s8.s32 "
            "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+r"(s0),"+r"(s1),"+r"(s2),"+r"(s3)
        : "r"(a.x),"r"(a.y),"r"(a.z),"r"(a.w),"r"(b.x),"r"(b.y));
    return (int4)(s0,s1,s2,s3);
}

__kernel void bonsai_pq2_mmq_int8(__global const uchar *weights, __global const uint *input,
                                 __global const float *scales, __global float *output,
                                 uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36],x[64][36];
    __local float weight_scale[64],token_scale[64];
    const uint blocks=width/128, row_base=64*work_group(), token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=32*(warp%2);
    const uint row=warp_row+lane/4, token=warp_token+2*(lane%4);
    float4 result[4];
    #pragma unroll
    for (uint i=0;i<4;++i) result[i]=(float4)(0.0f);

    for (uint block=0;block<blocks;++block) {
        for (uint packed=4*thread;packed<64*32;packed+=1024) {
            const uint t=token_base+packed/32;
            const uint size=t<tokens ? 16 : 0;
            const ulong from=size ? ((ulong)t*blocks+block)*32+packed%32 : 0;
            bonsai_copy16(&x[packed/32][packed%32],input+from,size);
        }
        if (thread<64) {
            const uint t=token_base+thread, r=row_base+thread;
            token_scale[thread]=t<tokens ? scales[(ulong)t*blocks+block] : 0.0f;
            float scale=0.0f;
            if (r<rows) {
                const __global ushort *encoded=(__global const ushort *)(weights+((ulong)r*blocks+block)*34);
                __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*encoded));
            }
            weight_scale[thread]=scale;
        }
        for (uint i=thread;i<64*32;i+=256) {
            const uint r=row_base+i/32;
            const uint code=r<rows ? weights[((ulong)r*blocks+block)*34+2+i%32] : 0x55u;
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
    for (uint f=0;f<4;++f) bonsai_store_mma(output,result[f],rows,tokens,row_base+row,token_base+token+8*f);
}
