static uint2 bonsai_ffn_unpack(__global const uchar *row,uint quad) {
    const uint codes=row[2+quad];
    float scale;
    __asm__("cvt.f32.f16 %0,%1;" : "=f"(scale) : "h"(*(__global const ushort *)row));
    return (uint2)(bonsai_half_pair(scale*(float)((int)(codes&3)-1),
                                   scale*(float)((int)((codes>>2)&3)-1)),
                  bonsai_half_pair(scale*(float)((int)((codes>>4)&3)-1),
                                   scale*(float)((int)(codes>>6)-1)));
}

static float bonsai_ffn_swiglu(float gate,float up) {
    return (gate/(1.0f+__nv_expf(-gate)))*up;
}

static float4 bonsai_ffn_result(float4 gate,float4 up) {
    return (float4)(bonsai_ffn_swiglu(gate.x,up.x),bonsai_ffn_swiglu(gate.y,up.y),
                    bonsai_ffn_swiglu(gate.z,up.z),bonsai_ffn_swiglu(gate.w,up.w));
}
