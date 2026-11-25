"""Generate fused variants retaining the wide tile's per-output operation order."""
from pathlib import Path

here=Path(__file__).resolve().parent
print((here/'ffn-helpers.cl').read_text())


def emit(m,dual):
    fragments=4 if m==64 else 2
    token_warps=8//(m//16)
    n=64
    alloc_y=f'    __local __attribute__((aligned(16))) uint u[{m}][68];' if dual else ''
    load_act='''        for (uint packed=thread*4;packed<64*64;packed+=1024) {
            const uint t=token_base+packed/64,col=packed%64;
            const uint valid=t<tokens ? 16 : 0;
            const ulong from=valid ? (ulong)t*(width/2)+block*64+col : 0;
            bonsai_copy16(&x[packed/64][col],input+from,valid);
        }'''
    pre='''        for (uint plane=0;plane<2;++plane) {
            const __global uchar *matrix=plane ? weight_up : weight_gate;''' if not dual else '''        const __global uchar *matrix=weight_gate;'''
    load_up='''            const uint2 up=r<rows ? bonsai_ffn_unpack(weight_up+from,quad) : (uint2)(0);
            u[i/32][quad*2]=up.x; u[i/32][quad*2+1]=up.y;''' if dual else ''
    wait='        bonsai_copy_complete();' if dual else '        if (plane==0) bonsai_copy_complete();'
    mma='''                const uint4 g=bonsai_load_a(&u[r0][0],68,k,lane);
                acc_gate[f]=bonsai_mma16x8(acc_gate[f],a.x,a.y,a.z,a.w,b.x,b.y);
                acc_up[f]=bonsai_mma16x8(acc_up[f],g.x,g.y,g.z,g.w,b.x,b.y);''' if dual else '''                if (plane==0) acc_gate[f]=bonsai_mma16x8(acc_gate[f],a.x,a.y,a.z,a.w,b.x,b.y);
                else acc_up[f]=bonsai_mma16x8(acc_up[f],a.x,a.y,a.z,a.w,b.x,b.y);'''
    close='        }' if not dual else ''
    print(f'''
__kernel void bonsai_ffn_fused_{m}x{n}(__global const uchar *weight_gate,
    __global const uchar *weight_up,__global const uint *input,__global float *output,
    uint width,uint rows,uint tokens) {{
    __local __attribute__((aligned(16))) uint w[{m}][68];
    __local __attribute__((aligned(16))) uint x[{n}][68];
{alloc_y}
    const uint blocks=width/128,row_base={m}*work_group(),token_base={n}*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint r0=16*(warp/{token_warps}),t0={fragments*8}*(warp%{token_warps});
    float4 acc_gate[{fragments}],acc_up[{fragments}];
    #pragma unroll
    for (uint f=0;f<{fragments};++f) {{ acc_gate[f]=(float4)(0); acc_up[f]=(float4)(0); }}
    for (uint block=0;block<blocks;++block) {{
{load_act}
{pre}
        for (uint i=thread;i<{m}*32;i+=256) {{
            const uint r=row_base+i/32,quad=i%32;
            const ulong from=((ulong)r*blocks+block)*34;
            const uint2 g=r<rows ? bonsai_ffn_unpack(matrix+from,quad) : (uint2)(0);
            w[i/32][quad*2]=g.x; w[i/32][quad*2+1]=g.y;
{load_up}
        }}
{wait}
        sync_threads();
        #pragma unroll
        for (uint k=0;k<64;k+=8) {{
            const uint4 a=bonsai_load_a(&w[r0][0],68,k,lane);
            #pragma unroll
            for (uint f=0;f<{fragments};++f) {{
                const uint2 b=bonsai_load_b(&x[t0+8*f][0],68,k,lane);
{mma}
            }}
        }}
        sync_threads();
{close}
    }}
    const uint row=row_base+r0+lane/4,token=token_base+t0+2*(lane%4);
    #pragma unroll
    for (uint f=0;f<{fragments};++f) {{
        bonsai_store_mma(output,bonsai_ffn_result(acc_gate[f],acc_up[f]),rows,tokens,row,token+8*f);
    }}
}}''')


emit(32,True)
emit(64,False)
