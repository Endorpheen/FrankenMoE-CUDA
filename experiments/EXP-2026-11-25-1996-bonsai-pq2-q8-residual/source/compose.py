"""Reuse the INT8 tile's weights across the main product and residual."""
from pathlib import Path
import sys

here=Path(__file__).resolve().parent
device=Path(sys.argv[1]).read_text()
tile=device.split('__kernel void bonsai_pq2_mmq_int8(',1)[1].split('\n}\n',1)[0]


def replace(old,new):
    global tile
    if tile.count(old)!=1:
        raise ValueError(old)
    tile=tile.replace(old,new)


replace('__global const uint *input,','__global const uint *input, __global const uint *correction,')
replace('uint w[64][36],x[64][36];','uint w[64][36],x[64][36],delta[64][36];')
replace('            bonsai_copy16(&x[packed/32][packed%32],input+from,size);',
'''            bonsai_copy16(&x[packed/32][packed%32],input+from,size);
            bonsai_copy16(&delta[packed/32][packed%32],correction+from,size);''')
replace('        int4 dot[4];','        int4 dot[4], low_dot[4];')
replace('        for (uint f=0;f<4;++f) dot[f]=(int4)(0);',
        '        for (uint f=0;f<4;++f) { dot[f]=(int4)(0); low_dot[f]=(int4)(0); }')
replace('                dot[f]=bonsai_mma_i8(dot[f],a,b);',
'''                const uint2 lo=bonsai_load_b(&delta[warp_token+8*f][0],36,k,lane);
                dot[f]=bonsai_mma_i8(dot[f],a,b);
                low_dot[f]=bonsai_mma_i8(low_dot[f],a,lo);''')
replace('            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;',
'''            const int4 lo=low_dot[f];
            const float4 hi=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w);
            const float4 residual=(float4)((float)lo.x,(float)lo.y,(float)lo.z,(float)lo.w);
            result[f]+=(hi+residual*(1.0f/254.0f))*scale;''')
print((here/'residual.cl').read_text())
print('__kernel void bonsai_pq2_mmq_q8_residual('+tile+'\n}')
