__kernel void bonsai_pack_q8_64(__global const float *input, __global uint *packed,
                                __global float *scales, uint groups) {
    const uint thread=local_id(),lane=thread%16;
    const uint group=8*work_group()+thread/16;
    const __global float *p=input+(ulong)(group<groups ? group : 0)*64+4*lane;
    const float4 v=(float4)(p[0],p[1],p[2],p[3]);
    const uint4 abs_bits=as_uint4(v)&(uint4)(0x7fffffffu);
    const float4 magnitude=as_float4(abs_bits);
    float peak=flash_max(flash_max(magnitude.x,magnitude.y),flash_max(magnitude.z,magnitude.w));
    for (uint bit=8;bit;bit/=2) peak=flash_max(peak,flash_shuffle(peak,bit));
    const float scale=peak/127.0f;
    if (group>=groups) return;
    packed[(ulong)group*16+lane]=bonsai_quantize_q8_value(v.x,scale)
        | (bonsai_quantize_q8_value(v.y,scale)<<8)
        | (bonsai_quantize_q8_value(v.z,scale)<<16)
        | (bonsai_quantize_q8_value(v.w,scale)<<24);
    if (lane==0) scales[group]=scale;
}
