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
