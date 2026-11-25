"""Generate E4M3 PV for the existing cumulative E4M3 QK Flash tile."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
source = base.read_text()
entry = "bonsai_flash_fp8_qk"
start = source.index(f"__kernel void {entry}(")
end = source.index("\n}\n", start) + 3
tile = source[start:end].replace(entry, "bonsai_flash_fp8_qkpv")
tile = replace(tile, "            uint probabilities[2];", "            ushort probabilities[2];")
tile = replace(tile, "                probabilities[z/64] = bonsai_half_pair(lo,hi);",
               """                ushort encoded;
                __asm__("cvt.rn.satfinite.e4m3x2.f32 %0,%2,%1;"
                    : "=h"(encoded) : "f"(lo*256.0f), "f"(hi*256.0f));
                probabilities[z/64] = encoded;""")
tile = replace(tile, """            probability[r][lane] = probabilities[0];
            probability[r][lane+32] = probabilities[1];""",
               """            __local ushort *packed = (__local ushort *)probability[r];
            packed[lane] = probabilities[0];
            packed[lane+32] = probabilities[1];""")
start = tile.index("        for (uint vb = 0;")
end = tile.index("        const uint token0", start)
tile = tile[:start] + """        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*2048 : 0;
            for (uint index = physical_thread*4; index < 2048; index += 1024) {
                bonsai_copy16(staging+(index/8)*12+index%8,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS,vb/4,lane);
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature_block = (32*result+8*warp)*12;
                const uint2 b = bonsai_load_b(staging+feature_block,12,0,lane);
                acc[result]=bonsai_mma_fp8(acc[result],a,b);
            }
            sync_threads();
        }
""" + tile[end:]
tile = replace(tile, "acc[result].xy*as_float(query_fp8[lane_row][67])",
               "(acc[result].xy*(1.0f/256.0f))*as_float(query_fp8[lane_row][67])")
tile = replace(tile, "acc[result].zw*as_float(query_fp8[lane_row+8][67])",
               "(acc[result].zw*(1.0f/256.0f))*as_float(query_fp8[lane_row+8][67])")
stage = """
static float bonsai_fp8_v_feature(__global const uchar *v,uint key,uint keys,uint head,uint feature) {
    if (key>=keys) return 0.0f;
    const float2 pair=flash_q4_pair(v+(ulong)key*576+head*144,feature/2);
    return feature&1 ? pair.y : pair.x;
}

__kernel void bonsai_flash_stage_v_fp8(__global const uchar *v,
                                      __global uint *packed_v,
                                      uint keys,uint chunks) {
    const uint block=work_group(),head=flash_grid_y(),thread=local_id();
    const ulong base=((ulong)head*chunks+block)*2048;
    for (uint i=thread;i<2048;i+=128) {
        const uint feature=i/8,key=block*32+4*(i%8);
        const float4 value=(float4)(bonsai_fp8_v_feature(v,key,keys,head,feature),
            bonsai_fp8_v_feature(v,key+1,keys,head,feature),
            bonsai_fp8_v_feature(v,key+2,keys,head,feature),
            bonsai_fp8_v_feature(v,key+3,keys,head,feature));
        packed_v[base+i]=bonsai_fp8_four(value);
    }
}
"""
out.write_text(source + "\n" + stage + "\n" + tile)
