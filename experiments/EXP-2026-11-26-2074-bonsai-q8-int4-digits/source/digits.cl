static uint bonsai_low_nibbles(uint packed) {
    return (packed&15u) | ((packed>>4)&240u) | ((packed>>8)&3840u) | ((packed>>12)&61440u);
}

__kernel void bonsai_pack_q8_digit_planes(__global const float *input, __global uint *upper,
    __global uint *lower, __global float *scales, uint groups) {
    const uint thread=local_id(),lane=thread%32;
    const uint group=4*work_group()+thread/32;
    if (group>=groups) return;
    const __global float *x=input+(ulong)group*128+4*lane;
    const float4 v=(float4)(x[0],x[1],x[2],x[3]);
    const float4 a=as_float4(as_uint4(v)&(uint4)(0x7fffffffu));
    float peak=flash_max(flash_max(a.x,a.y),flash_max(a.z,a.w));
    for (uint bit=16;bit;bit/=2) peak=flash_max(peak,flash_shuffle(peak,bit));
    const float scale=peak/127.0f;
    const uint q=bonsai_quantize_q8_value(v.x,scale)
        | (bonsai_quantize_q8_value(v.y,scale)<<8)
        | (bonsai_quantize_q8_value(v.z,scale)<<16)
        | (bonsai_quantize_q8_value(v.w,scale)<<24);
    const uint lo=bonsai_low_nibbles(q),hi=bonsai_low_nibbles(q>>4);
    uint next_lo,next_hi;
    __asm__("shfl.sync.bfly.b32 %0, %1, 1, 31, -1;" : "=r"(next_lo) : "r"(lo));
    __asm__("shfl.sync.bfly.b32 %0, %1, 1, 31, -1;" : "=r"(next_hi) : "r"(hi));
    if (!(lane&1)) {
        const ulong at=(ulong)group*16+lane/2;
        lower[at]=lo|(next_lo<<16);
        upper[at]=hi|(next_hi<<16);
    }
    if (lane==0) scales[group]=scale;
}

static int4 bonsai_mma_digit_signed(int4 sum,uint4 a,uint2 b) {
    int s0=sum.x,s1=sum.y,s2=sum.z,s3=sum.w;
    __asm__("mma.sync.aligned.m16n8k64.row.col.s32.s4.s4.s32 "
            "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+r"(s0),"+r"(s1),"+r"(s2),"+r"(s3)
        : "r"(a.x),"r"(a.y),"r"(a.z),"r"(a.w),"r"(b.x),"r"(b.y));
    return (int4)(s0,s1,s2,s3);
}

static int4 bonsai_mma_digit_unsigned(int4 sum,uint4 a,uint2 b) {
    int s0=sum.x,s1=sum.y,s2=sum.z,s3=sum.w;
    __asm__("mma.sync.aligned.m16n8k64.row.col.s32.s4.u4.s32 "
            "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+r"(s0),"+r"(s1),"+r"(s2),"+r"(s3)
        : "r"(a.x),"r"(a.y),"r"(a.z),"r"(a.w),"r"(b.x),"r"(b.y));
    return (int4)(s0,s1,s2,s3);
}
