static float bonsai_rn_half(float f) {
    float out;
    __asm__("{ .reg .b16 h; cvt.rn.f16.f32 h,%1; cvt.f32.f16 %0,h; }"
        : "=f"(out) : "f"(f));
    return out;
}

static uint bonsai_q8_pack4(float4 v,float step) {
    return bonsai_quantize_q8_value(v.x,step) |
        (bonsai_quantize_q8_value(v.y,step)<<8) |
        (bonsai_quantize_q8_value(v.z,step)<<16) |
        (bonsai_quantize_q8_value(v.w,step)<<24);
}

static float bonsai_signed_byte(uint byte) {
    const uint u=byte&255u;
    return (float)((int)u-((u&128u)?256:0));
}

__kernel void bonsai_pack_q8_residual(__global const float *input, __global uint *main_codes,
    __global uint *residual_codes, __global float *scales, uint groups) {
    const uint thread=local_id(),lane=thread%32;
    const uint row=4*work_group()+thread/32;
    if (row>=groups) return;
    const __global float *x=input+(ulong)row*128+4*lane;
    const float4 v=(float4)(bonsai_rn_half(x[0]),bonsai_rn_half(x[1]),bonsai_rn_half(x[2]),bonsai_rn_half(x[3]));
    float largest=as_float(as_uint(v.x)&0x7fffffffu);
    largest=flash_max(largest,as_float(as_uint(v.y)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.z)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.w)&0x7fffffffu));
    for (uint bit=16;bit;bit/=2) largest=flash_max(largest,flash_shuffle(largest,bit));
    const float step=largest/127.0f, correction=step/254.0f;
    const uint hi=bonsai_q8_pack4(v,step);
    const float4 decoded=(float4)(bonsai_signed_byte(hi),bonsai_signed_byte(hi>>8),
                                  bonsai_signed_byte(hi>>16),bonsai_signed_byte(hi>>24));
    const float4 residual=v-decoded*step;
    const ulong at=(ulong)row*32+lane;
    main_codes[at]=hi;
    residual_codes[at]=bonsai_q8_pack4(residual,correction);
    if (lane==0) scales[row]=step;
}
