static uint bonsai_residual_nibble(float value, float step) {
    int code = 0;
    if (step > 0.0f) {
        const float normalized = value/step;
        __asm__("cvt.rni.s32.f32 %0, %1;" : "=r"(code) : "f"(normalized));
        code = code < -7 ? -7 : (code > 7 ? 7 : code);
    }
    return (uint)code & 15u;
}

static uint bonsai_narrow_four(uint bytes) {
    return (bytes&15u) | ((bytes>>4)&240u) | ((bytes>>8)&3840u) | ((bytes>>12)&61440u);
}

__kernel void bonsai_pack_q8_q4_residual(__global const float *input, __global uint *main_codes,
    __global uint *residual_codes, __global float *scales, uint groups) {
    const uint thread=local_id(),lane=thread%32;
    const uint row=4*work_group()+thread/32;
    if (row>=groups) return;
    const __global float *x=input+(ulong)row*128+4*lane;
    const float4 v=(float4)(bonsai_rn_half(x[0]),bonsai_rn_half(x[1]),
                            bonsai_rn_half(x[2]),bonsai_rn_half(x[3]));
    float largest=as_float(as_uint(v.x)&0x7fffffffu);
    largest=flash_max(largest,as_float(as_uint(v.y)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.z)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.w)&0x7fffffffu));
    for (uint bit=16;bit;bit/=2) largest=flash_max(largest,flash_shuffle(largest,bit));
    const float step=largest/127.0f, residual_step=step/14.0f;
    const uint hi=bonsai_q8_pack4(v,step);
    const float4 decoded=(float4)(bonsai_signed_byte(hi),bonsai_signed_byte(hi>>8),
                                  bonsai_signed_byte(hi>>16),bonsai_signed_byte(hi>>24));
    const float4 r=v-decoded*step;
    const uint lo=bonsai_residual_nibble(r.x,residual_step) |
        (bonsai_residual_nibble(r.y,residual_step)<<4) |
        (bonsai_residual_nibble(r.z,residual_step)<<8) |
        (bonsai_residual_nibble(r.w,residual_step)<<12);
    uint adjacent;
    __asm__("shfl.sync.bfly.b32 %0, %1, 1, 31, -1;" : "=r"(adjacent) : "r"(lo));
    main_codes[(ulong)row*32+lane]=hi;
    if (!(lane&1)) residual_codes[(ulong)row*16+lane/2]=lo|(adjacent<<16);
    if (lane==0) scales[row]=step;
}

static int4 bonsai_mma_i4(int4 sum, uint4 a, uint2 b) {
    int s0=sum.x,s1=sum.y,s2=sum.z,s3=sum.w;
    __asm__("mma.sync.aligned.m16n8k64.row.col.s32.s4.s4.s32 "
            "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+r"(s0),"+r"(s1),"+r"(s2),"+r"(s3)
        : "r"(a.x),"r"(a.y),"r"(a.z),"r"(a.w),"r"(b.x),"r"(b.y));
    return (int4)(s0,s1,s2,s3);
}
