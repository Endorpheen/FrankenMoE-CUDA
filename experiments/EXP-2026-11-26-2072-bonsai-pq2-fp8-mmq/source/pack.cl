static uint bonsai_expand_pq2_fp8(uint code) {
    const uint selector=(code&3u)|((code&12u)<<2)|((code&48u)<<4)|((code&192u)<<6);
    uint packed;
    __asm__("prmt.b32 %0, %1, %1, %2;" : "=r"(packed) : "r"(0x403800b8u), "r"(selector));
    return packed;
}

__kernel void bonsai_pack_pq2_fp8(__global const float *input, __global uint *packed,
                                  __global float *scales, uint groups) {
    const uint thread=local_id(),lane=thread%32;
    const uint group=4*work_group()+thread/32;
    if (group>=groups) return;
    const __global float *row=input+(ulong)group*128+4*lane;
    const float4 v=(float4)(row[0],row[1],row[2],row[3]);
    const float4 a=as_float4(as_uint4(v)&(uint4)(0x7fffffffu));
    float peak=flash_max(flash_max(a.x,a.y),flash_max(a.z,a.w));
    for (uint bit=16;bit;bit/=2) peak=flash_max(peak,flash_shuffle(peak,bit));
    const float scale=peak/448.0f;
    packed[(ulong)group*32+lane]=scale>0.0f ? bonsai_fp8_four(v/scale) : 0;
    if (lane==0) scales[group]=scale;
}
