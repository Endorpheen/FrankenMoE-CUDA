"""Generate an experimental E4M3 QK path with the existing FP16 PV."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
source = base.read_text()
start = source.index("__kernel void bonsai_flash_kv_fused_ordered(")
end = source.index("\n}\n", start) + 3
tile = source[start:end].replace("bonsai_flash_kv_fused_ordered", "bonsai_flash_fp8_qk")

tile = replace(tile, "panel_scratch[12544]", "panel_scratch[10368]")
tile = replace(tile, "(*query_half)[132] = (__local uint (*)[132])(panel_scratch + panel * 2112)",
               "(*query_fp8)[68] = (__local uint (*)[68])(panel_scratch + panel * 1088)")
tile = replace(tile, "panel_scratch + 4224 + panel * 2048", "panel_scratch + 2176 + panel * 2048")
tile = replace(tile, "panel_scratch + 8320", "panel_scratch + 6272")
tile = replace(tile, "FLASH_ROWS * 128", "FLASH_ROWS * 64")
tile = replace(tile, "const uint r = index / 128, col = (index % 128)*2;",
               "const uint r = index / 64, col = (index % 64)*4;")
tile = replace(tile, "packed = bonsai_half_pair(q[0], q[1]);",
               "packed = bonsai_fp8_four((float4)(q[0],q[1],q[2],q[3]));")
tile = replace(tile, "query_half[r][index%128]", "query_fp8[r][index%64]")
for src, dst in ((128, 64), (129, 65), (130, 66), (131, 67)):
    tile = tile.replace(f"][{src}]", f"][{dst}]")
tile = tile.replace("query_half", "query_fp8")
tile = replace(tile, """const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);""",
               """const ulong base = available ? ((ulong)kv_head*chunks+chunk)*2048 : 0;
            for (uint index = physical_thread*4; index < 32*64; index += 1024) {
                bonsai_copy16(staging+(index/64)*68+index%64,keys+base+index,available);""")
tile = replace(tile, """            #pragma unroll 8
            for (uint k = 0; k < 128; k += 8) {
                const uint4 a = bonsai_load_a(&query_fp8[0][0],132,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
                acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);""",
               """            #pragma unroll 8
            for (uint k = 0; k < 64; k += 8) {
                const uint4 a = bonsai_load_a(&query_fp8[0][0],68,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*68,68,k,lane);
                acc = bonsai_mma_fp8(acc,a,b);""")
out.write_text(source + "\n" + Path(__file__).with_name("fp8.cl").read_text() + "\n" + tile)
