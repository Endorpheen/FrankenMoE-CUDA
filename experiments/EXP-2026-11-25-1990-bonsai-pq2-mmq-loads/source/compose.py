"""Build PQ2 MMQ data-movement variants with identical FP32 accumulation."""
from pathlib import Path
import sys

here=Path(__file__).resolve().parent
device=Path(sys.argv[1]).read_text()
text=device.split('__kernel void bonsai_pq2_mmq(',1)[1].split('\n}\n',1)[0]


def rep(text, old, new):
    if text.count(old)!=1:
        raise ValueError(old)
    return text.replace(old,new)


text=rep(text, '    __local uint w[32][68];\n    __local uint x[32][68];',
    '''    __local __attribute__((aligned(16))) uint w[32][68];
    __local __attribute__((aligned(16))) uint x[32][68];''')
text=rep(text,'    const uint t = 16 * (warp % 2) + lane / 4;','    const uint t = 16 * (warp % 2);')
text=rep(text,'''            const uint p = k + pair;
            const uint a0 = w[r][p], a1 = w[r + 8][p], a2 = w[r][p + 4], a3 = w[r + 8][p + 4];
            left = bonsai_mma16x8(left, a0, a1, a2, a3, x[t][p], x[t][p + 4]);
            right = bonsai_mma16x8(right, a0, a1, a2, a3, x[t + 8][p], x[t + 8][p + 4]);''',
'''            const uint4 a = bonsai_load_a(&w[16*(warp/2)][0],68,k,lane);
            const uint2 b0 = bonsai_load_b(&x[t][0],68,k,lane);
            const uint2 b1 = bonsai_load_b(&x[t+8][0],68,k,lane);
            left = bonsai_mma16x8(left,a.x,a.y,a.z,a.w,b0.x,b0.y);
            right = bonsai_mma16x8(right,a.x,a.y,a.z,a.w,b1.x,b1.y);''')
print('__kernel void bonsai_pq2_mmq_ldmatrix('+text+'\n}')
text=rep(text,'    for (uint block = 0; block < blocks; ++block) {',
'''    for (uint block = 0; block < blocks; ++block) {
        for (uint packed = thread*4; packed < 32*64; packed += 512) {
            const uint token = token_base+packed/64, col = packed%64;
            const uint valid = token < tokens ? 16 : 0;
            const ulong source = valid ? (ulong)token*(width/2)+block*64+col : 0;
            bonsai_copy16(&x[packed/64][col],input+source,valid);
        }''')
text=rep(text,'            const uint row = row_base + local_row, token = token_base + local_row;',
         '            const uint row = row_base + local_row;')
text=rep(text,'            uint a0 = 0, a1 = 0, b0 = 0, b1 = 0;','            uint a0 = 0, a1 = 0;')
text=rep(text,'''            if (token < tokens) {
                const ulong col = (ulong)token * (width / 2) + block * 64 + quad * 2;
                b0 = input[col];
                b1 = input[col + 1];
            }
''','')
text=rep(text,'''            x[local_row][quad * 2] = b0;
            x[local_row][quad * 2 + 1] = b1;
        }
        sync_threads();''','''        }
        bonsai_copy_complete();
        sync_threads();''')
print('__kernel void bonsai_pq2_mmq_async('+text+'\n}')
text=rep(text,'''                float scale;
                __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(scale_bits));
                a0 = bonsai_half_pair(scale * (float)((int)(codes & 3) - 1),
                                      scale * (float)((int)((codes >> 2) & 3) - 1));
                a1 = bonsai_half_pair(scale * (float)((int)((codes >> 4) & 3) - 1),
                                      scale * (float)((int)(codes >> 6) - 1));''',
'''                const uint2 operands = bonsai_pq2_quad(scale_bits,codes);
                a0 = operands.x;
                a1 = operands.y;''')
print((here/'half-lookup.cl').read_text())
print('__kernel void bonsai_pq2_mmq_lookup('+text+'\n}')
