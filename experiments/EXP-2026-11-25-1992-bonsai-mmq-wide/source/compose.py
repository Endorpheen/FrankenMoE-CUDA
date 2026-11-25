"""Vary the matrix tile without reordering FP32 updates within each output."""
from pathlib import Path
import sys

src=Path(sys.argv[1]).read_text()
body=src.split('__kernel void bonsai_pq2_mmq_async(',1)[1].split('\n}\n',1)[0]


def replace(text, old, new):
    if text.count(old)!=1:
        raise ValueError(old)
    return text.replace(old,new)


def emit(m,n):
    token_warps=8//(m//16)
    fragments=n//(token_warps*8)
    text=replace(body,'uint w[32][68]',f'uint w[{m}][68]')
    text=replace(text,'uint x[32][68]',f'uint x[{n}][68]')
    text=replace(text,'row_base = 32 * work_group(), token_base = 32 * tile_token',
        f'row_base = {m} * work_group(), token_base = {n} * tile_token')
    text=replace(text,'const uint r = 16 * (warp / 2) + lane / 4;',
        f'const uint r = 16 * (warp / {token_warps}) + lane / 4;')
    text=replace(text,'const uint t = 16 * (warp % 2);',
        f'const uint t = {fragments*8} * (warp % {token_warps});')
    text=replace(text,'    float4 left = (float4)(0.0f), right = (float4)(0.0f);',
        f'''    float4 acc[{fragments}];
    #pragma unroll
    for (uint i=0; i<{fragments}; ++i) acc[i]=(float4)(0.0f);''')
    text=replace(text,'packed < 32*64; packed += 512',f'packed < {n}*64; packed += 1024')
    text=replace(text,'index < 32 * 32; index += 128',f'index < {m} * 32; index += 256')
    start=text.index('            const uint4 a =')
    stop=text.index('        }\n        sync_threads();',start)
    text=text[:start]+f'''            const uint4 a = bonsai_load_a(&w[16*(warp/{token_warps})][0],68,k,lane);
            #pragma unroll
            for (uint i=0; i<{fragments}; ++i) {{
                const uint2 b = bonsai_load_b(&x[t+8*i][0],68,k,lane);
                acc[i] = bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }}
'''+text[stop:]
    text=text[:text.index('    const uint out_token =')]+f'''    const uint out_token = token_base + t + 2*pair;
    #pragma unroll
    for (uint i=0; i<{fragments}; ++i) {{
        bonsai_store_mma(output,acc[i],rows,tokens,row_base+r,out_token+8*i);
    }}
'''
    print(f'__kernel void bonsai_pq2_mmq_{m}x{n}('+text+'\n}')


for shape in ((32,64),(64,32),(64,64)):
    emit(*shape)
