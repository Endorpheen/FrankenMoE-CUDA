"""Generate exact full tiles for the accepted Bonsai Q8 gate/up path."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


def kernel(control, tile):
    fragments = tile // 16
    code = control.replace("bonsai_pq2_mmq_int8", f"bonsai_pq2_mmq_int8_full_n{tile}")
    code = replace(code, "uint width,uint rows,uint tokens)", "uint width,uint output_rows,uint tokens)")
    code = replace(code, "uint w[64][36],x[64][36]", f"uint w[64][36],x[{tile}][36]")
    code = replace(code, "weight_scale[64],token_scale[64]", f"weight_scale[64],token_scale[{tile}]")
    code = replace(code, "const uint blocks=width/128, row_base=64*work_group(), token_base=64*flash_grid_y();",
                   f"const uint rows=17408,blocks=40,row_base=64*work_group(),token_base={tile}*flash_grid_y();")
    code = replace(code, "warp_token=32*(warp%2)", f"warp_token={tile//2}*(warp%2)")
    code = replace(code, "float4 result[4];", f"float4 result[{fragments}];")
    code = replace(code, "i<4;++i", f"i<{fragments};++i")
    code = replace(code, "packed<64*32", f"packed<{tile}*32")
    code = replace(
        code,
        """            const uint size=t<tokens ? 16 : 0;
            const ulong from=size ? ((ulong)t*blocks+block)*32+packed%32 : 0;
            bonsai_copy16(&x[packed/32][packed%32],input+from,size);""",
        """            const ulong from=((ulong)t*blocks+block)*32+packed%32;
            bonsai_copy16(&x[packed/32][packed%32],input+from,16);""",
    )
    start = code.index("        if (thread<64)")
    stop = code.index("        for (uint i=thread;i<64*32", start)
    code = code[:start] + f"""        if (thread<{tile}) {{
            const uint token=token_base+thread;
            token_scale[thread]=scales[(ulong)token*blocks+block];
        }}
        if (thread<64) {{
            const uint row=row_base+thread;
            const __global ushort *encoded=(__global const ushort *)(weights+((ulong)row*blocks+block)*34);
            float scale;
            __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*encoded));
            weight_scale[thread]=scale;
        }}
""" + code[stop:]
    code = replace(code, "r<rows ? weights[((ulong)r*blocks+block)*34+2+i%32] : 0x55u",
                   "weights[((ulong)r*blocks+block)*34+2+i%32]")
    code = replace(code, "int4 dot[4];", f"int4 dot[{fragments}];")
    assert code.count("f<4;++f") == 4
    code = code.replace("f<4;++f", f"f<{fragments};++f")
    return replace(code, f"    for (uint f=0;f<{fragments};++f) bonsai_store_mma(output,result[f],rows,tokens,row_base+row,token_base+token+8*f);",
                   f"""    for (uint f=0;f<{fragments};++f) {{
        const ulong offset=(ulong)(token_base+token+8*f)*rows+row_base+row;
        output[offset]=result[f].x;
        output[offset+8]=result[f].z;
        output[offset+rows]=result[f].y;
        output[offset+rows+8]=result[f].w;
    }}""")


base, out = map(Path, sys.argv[1:])
source = base.read_text()
start = source.index("__kernel void bonsai_pq2_mmq_int8(")
stop = source.index("\n}\n", start) + 3
out.write_text(source + "\n".join(kernel(source[start:stop], tile) for tile in (64, 128)))
