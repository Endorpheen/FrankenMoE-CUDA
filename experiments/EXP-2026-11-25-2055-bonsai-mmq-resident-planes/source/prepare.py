"""Generate K64 that reads aligned pairs from resident PQ2 code and scale planes."""

from pathlib import Path
import sys


base, out = map(Path, sys.argv[1:])
source = base.read_text()
start = source.index("__kernel void bonsai_pq2_cached_64x128_k64(")
end = source.index("\n}\n", start) + 3
kernel = source[start:end].replace("bonsai_pq2_cached_64x128_k64", "bonsai_pq2_cached_64x128_planes")
signature = "__global const uchar *weights,"
assert kernel.count(signature) == 1
kernel = kernel.replace(signature, "__global const ushort *codes_plane, __global const ushort *scale_plane,")
old = """                uint bits=0,low=0,high=0;
                if (row<rows) {
                    const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                    bits=*(__global const ushort *)q;
                    low=q[2+quad];
                    high=q[18+quad];
                }
                raw[lane_row]=bits|(low<<16)|(high<<24);"""
new = """                uint pair=0,bits=0;
                if (row<rows) {
                    const ulong index=(ulong)(step/2)*rows+row;
                    pair=codes_plane[index*16+quad];
                    bits=scale_plane[index];
                }
                raw[lane_row]=bits|(pair<<16);"""
assert kernel.count(old) == 1
pack = """
__kernel void bonsai_pack_pq2_resident_planes(__global const uchar *weights,
    __global ushort *codes_plane, __global ushort *scale_plane,
    uint rows,uint blocks,uint entries) {
    const uint i=work_group()*256+local_id();
    if (i>=entries*16) return;
    const uint entry=i/16,quad=i%16;
    const uint row=entry/blocks,block=entry%blocks;
    const ulong destination=(ulong)block*rows+row;
    const __global uchar *src=weights+(ulong)entry*34;
    codes_plane[destination*16+quad]=(ushort)(src[2+quad]|((uint)src[18+quad]<<8));
    if (quad==0) scale_plane[destination]=*(__global const ushort *)src;
}
"""
out.write_text(source + "\n" + kernel.replace(old, new) + pack)
