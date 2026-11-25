"""Generate a dense half-input form from the qualified wide PQ2 MMQ tile."""
from pathlib import Path
import sys

p=Path(sys.argv[1])
text=p.read_text().split('__kernel void bonsai_pq2_mmq_64x64(',1)[1].split('\n}\n',1)[0]
text=text.replace('__global const uchar *weights','__global const uint *weights',1)
start=text.index('        for (uint index = thread;')
end=text.index('        bonsai_copy_complete();',start)
text=text[:start]+'''        for (uint packed = thread*4; packed < 64*64; packed += 1024) {
            const uint row = row_base+packed/64, col = packed%64;
            const uint valid = row < rows ? 16 : 0;
            const ulong source = valid ? (ulong)row*(width/2)+block*64+col : 0;
            bonsai_copy16(&w[packed/64][col],weights+source,valid);
        }
'''+text[end:]
print((Path(__file__).resolve().parent/'expand.cl').read_text())
print('__kernel void bonsai_pq2_mmq_expanded('+text+'\n}')

plane=p.read_text().split('__kernel void bonsai_pq2_mmq_64x64(',1)[1].split('\n}\n',1)[0]
plane=plane.replace('__global const uchar *weights,','__global const uchar *codes_plane, __global const ushort *scale_plane,',1)
start=plane.index('                const __global uchar *q =')
end=plane.index('                float scale;',start)
plane=plane[:start]+'''                const uint index = block*rows+row;
                const uint codes = codes_plane[(ulong)index*32+quad];
                const ushort scale_bits = scale_plane[index];
'''+plane[end:]
print('__kernel void bonsai_pq2_mmq_planes('+plane+'\n}')
