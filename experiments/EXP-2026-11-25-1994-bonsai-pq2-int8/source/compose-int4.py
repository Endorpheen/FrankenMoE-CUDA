"""Create the corresponding INT4 path using the same geometry and block scales."""
from pathlib import Path
import sys

text=Path(sys.argv[1]).read_text()
q=text.split('static uint bonsai_quantize_q8_value(',1)[1].split('\n}\n',1)[0]
q=q.replace('-127','-7').replace('127','7').replace('&255u','&15u')
print('static uint bonsai_quantize_q4_value('+q+'\n}')
q=text.split('__kernel void bonsai_pack_q8_128(',1)[1].split('\n}\n',1)[0]
q=q.replace('__global uint *packed','__global ushort *packed').replace('127.0f','7.0f')
q=q.replace('bonsai_quantize_q8_value','bonsai_quantize_q4_value')
q=q.replace('<<8)', '<<4)').replace('<<16)', '<<8)').replace('<<24)', '<<12)')
print('__kernel void bonsai_pack_q4_128('+q+'\n}')
print('''
static uint bonsai_compact_nibbles(uint x) {
    return (x&15u) | ((x>>4)&0xf0u) | ((x>>8)&0xf00u) | ((x>>12)&0xf000u);
}

static uint bonsai_expand_pq2_nibbles(uint lo,uint hi) {
    return bonsai_compact_nibbles(bonsai_expand_pq2_byte(lo)) |
          (bonsai_compact_nibbles(bonsai_expand_pq2_byte(hi))<<16);
}
''')
x=text.split('static int4 bonsai_mma_i8(',1)[1].split('\n}\n',1)[0]
x=x.replace('m16n8k32.row.col.s32.s8.s8.s32','m16n8k64.row.col.s32.s4.s4.s32')
print('static int4 bonsai_mma_i4('+x+'\n}')
k=text.split('__kernel void bonsai_pq2_mmq_int8(',1)[1].split('\n}\n',1)[0]
k=k.replace('[64][36]','[64][20]').replace('packed<64*32','packed<64*16')
k=k.replace('packed/32','packed/16').replace('packed%32','packed%16')
k=k.replace('blocks+block)*32','blocks+block)*16').replace('i<64*32','i<64*16')
k=k.replace('i/32','i/16').replace('i%32','i%16')
a=k.index('            const uint code=')
b=k.index('\n        }',a)
k=k[:a]+'''            uint lo=0x55u,hi=0x55u;
            if (r<rows) {
                const __global uchar *q=weights+((ulong)r*blocks+block)*34+2+2*(i%16);
                lo=q[0];hi=q[1];
            }
            w[i/16][i%16]=bonsai_expand_pq2_nibbles(lo,hi);'''+k[b:]
k=k.replace('k<32','k<16').replace(',36,k,lane',',20,k,lane').replace('bonsai_mma_i8','bonsai_mma_i4')
print('__kernel void bonsai_pq2_mmq_int4('+k+'\n}')
