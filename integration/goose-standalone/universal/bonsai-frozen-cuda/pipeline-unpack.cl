#include "pipeline-mma-common.clh"

static inline uint bonsai_pq2_permute(uint a, uint b, uint selector) {
    uint result;
    __asm__("prmt.b32 %0, %1, %2, %3;" : "=r"(result) : "r"(a), "r"(b), "r"(selector));
    return result;
}

static inline uint2 bonsai_pq2_half2(ushort bits, uint code) {
    const uint selector = (code & 3u) | ((code & 12u) << 2) |
        ((code & 48u) << 4) | ((code & 192u) << 6);
    const uint factors = bonsai_pq2_permute(0x403c00bcu, 0, selector);
    const uint a = bonsai_pq2_permute(factors, 0, 0x1404);
    const uint b = bonsai_pq2_permute(factors, 0, 0x3424);
    const uint scale = (uint)bits | ((uint)bits << 16);
    uint first, second;
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(first) : "r"(a), "r"(scale));
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(second) : "r"(b), "r"(scale));
    return (uint2)(first, second);
}

__kernel void bonsai_pq2_half_unpack_lut_5120(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=40;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*2560+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_quad((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_half2_5120(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=40;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*2560+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_half2((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_lut_6144(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=48;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*3072+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_quad((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_half2_6144(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=48;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*3072+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_half2((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_lut_17408(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=136;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*8704+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_quad((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_half2_17408(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=136;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*8704+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_half2((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}
__kernel void bonsai_pq2_half_unpack_half2_17408_k128(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint paired[2][192][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=136;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    for (uint block=0;block<blocks;++block) {
        #pragma unroll
        for (uint half_index=0;half_index<2;++half_index) {
            for (uint packed=thread*4;packed<128*32;packed+=1024) {
                const uint token=token_base+packed/32,col=packed%32;
                const ulong source=(ulong)token*8704+(2*block+half_index)*32+col;
                bonsai_copy16(&paired[half_index][64+packed/32][col],input+source,16);
            }
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row;
            const uint row=row_base+index/16, quad=index%16;
            const __global uchar *q=weights+((ulong)row*blocks+block)*34;
            const ushort bits=*(__global const ushort *)q;
            const uint2 first=bonsai_pq2_half2(bits,q[2+quad]);
            const uint2 second=bonsai_pq2_half2(bits,q[18+quad]);
            *(__local uint2 *)&paired[0][index/16][quad*2]=first;
            *(__local uint2 *)&paired[1][index/16][quad*2]=second;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint half_index=0;half_index<2;++half_index) {
            #pragma unroll
            for (uint segment=0;segment<32;segment+=16) {
                uint2 partial[8];
                #pragma unroll
                for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
                #pragma unroll
                for (uint sub=0;sub<2;++sub) {
                    const uint k=segment+8*sub;
                    const uint4 a=bonsai_load_a(&paired[half_index][warp_row][0],36,k,lane);
                    #pragma unroll
                    for (uint i=0;i<8;++i) {
                        const uint2 b=bonsai_load_b(&paired[half_index][64+warp_token+8*i][0],36,k,lane);
                        partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                    }
                }
                #pragma unroll
                for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}
