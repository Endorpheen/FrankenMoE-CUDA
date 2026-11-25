"""Exact MMQ shapes with 64 consecutive K elements per shared stage."""


def tile(rows,tokens):
    token_warps=8//(rows//16)
    fragments=tokens//(8*token_warps)
    return f'''
__kernel void bonsai_pq2_mmq_{rows}x{tokens}_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {{
    __local __attribute__((aligned(16))) uint w[{rows}][36];
    __local __attribute__((aligned(16))) uint x[{tokens}][36];
    const uint row_base={rows}*work_group(), token_base={tokens}*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/{token_warps}),warp_token={fragments*8}*(warp%{token_warps});
    const uint blocks=width/128;
    float4 acc[{fragments}];
    #pragma unroll
    for (uint i=0;i<{fragments};++i) acc[i]=(float4)(0.0f);
    for (uint step=0;step<2*blocks;++step) {{
        for (uint packed=thread*4;packed<{tokens}*32;packed+=1024) {{
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }}
        for (uint index=thread;index<{rows}*16;index+=256) {{
            const uint row=row_base+index/16, quad=index%16;
            uint2 pair=(uint2)(0);
            if (row<rows) {{
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                pair=bonsai_ffn_unpack(q,quad+16*(step&1));
            }}
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }}
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint k=0;k<32;k+=8) {{
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<{fragments};++i) {{
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }}
        }}
        sync_threads();
    }}
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<{fragments};++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}}
'''


for rows,tokens in ((64,64),(64,128),(128,64),(128,128)):
    print(tile(rows,tokens))
