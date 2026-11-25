"""Generate a twelve-warp N192 tile for the cumulative Bonsai arithmetic."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


def variant(source, width):
    name = f"bonsai_pq2_half_full_{width}"
    start = source.index(f"__kernel void {name}(")
    end = source.index("\n}\n", start) + 3
    code = source[start:end].replace(name, f"bonsai_pq2_half_n192_{width}")
    code = replace(code, "__kernel void ", "__kernel __attribute__((reqd_work_group_size(384,1,1))) void ")
    code = replace(code, "uint x[128][36];", "uint x[192][36];")
    code = replace(code, "token_base=128*flash_grid_y()", "token_base=192*flash_grid_y()")
    code = replace(code, "warp_row=16*(warp/2),warp_token=64*(warp%2);",
                   "warp_row=16*(warp/3),warp_token=64*(warp%3);\n"
                   "    const int live_warp=token_base+warp_token<tokens;")
    code = replace(code, "    uint raw[4];", "    uint raw[3];")
    assert code.count("lane_row<4") == 2 and code.count("256*lane_row") == 2
    code = code.replace("lane_row<4", "lane_row<3").replace("256*lane_row", "384*lane_row")
    code = replace(code, """                const uint index=thread+384*lane_row;
                const uint row=""", """                const uint index=thread+384*lane_row;
                if (index>=64*16) continue;
                const uint row=""")
    code = replace(code, """            const uint index=thread+384*lane_row, quad=index%16;""",
                   """            const uint index=thread+384*lane_row;
            if (index>=64*16) continue;
            const uint quad=index%16;""")
    start = code.index("        for (uint packed=thread*4;")
    end = code.index("        #pragma unroll", start)
    code = code[:start] + f"""        for (uint packed=thread*4;packed<192*32;packed+=1536) {{
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*{width // 2}+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }}
""" + code[end:]
    start = code.index("        #pragma unroll\n        for (uint segment=0;")
    end = code.index("        sync_threads();", start)
    body = "\n".join("    " + line for line in code[start:end].rstrip().splitlines())
    code = code[:start] + "        if (live_warp) {\n" + body + "\n        }\n" + code[end:]
    start = code.index("    #pragma unroll\n    for (uint i=0;i<8;++i) {\n        const ulong offset")
    return code[:start] + """    #pragma unroll
    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}
"""


if __name__ == "__main__":
    base, out = map(Path, sys.argv[1:])
    source = base.read_text()
    out.write_text(source + "\n".join(variant(source, width) for width in (5120, 17408)))
