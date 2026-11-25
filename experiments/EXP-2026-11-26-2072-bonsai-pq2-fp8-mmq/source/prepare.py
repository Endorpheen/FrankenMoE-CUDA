"""Generate one E4M3 product per PQ2 block with FP32 scale application."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


def kernel(control, tokens):
    fragments = tokens // 16
    code = control.replace("bonsai_pq2_mmq_int8", f"bonsai_pq2_mmq_fp8_n{tokens}")
    code = replace(code, "uint w[64][36],x[64][36]", f"uint w[64][36],x[{tokens}][36]")
    code = replace(code, "weight_scale[64],token_scale[64]", f"weight_scale[64],token_scale[{tokens}]")
    code = replace(code, "token_base=64*flash_grid_y()", f"token_base={tokens}*flash_grid_y()")
    code = replace(code, "warp_token=32*(warp%2)", f"warp_token={tokens // 2}*(warp%2)")
    code = replace(code, "float4 result[4];", f"float4 result[{fragments}];")
    code = replace(code, "i<4;++i", f"i<{fragments};++i")
    code = replace(code, "packed<64*32", f"packed<{tokens}*32")
    code = replace(code, """        if (thread<64) {
            const uint t=token_base+thread, r=row_base+thread;
            token_scale[thread]=t<tokens ? scales[(ulong)t*blocks+block] : 0.0f;""",
                   f"""        if (thread<{tokens}) {{
            const uint t=token_base+thread;
            token_scale[thread]=t<tokens ? scales[(ulong)t*blocks+block] : 0.0f;
        }}
        if (thread<64) {{
            const uint r=row_base+thread;""")
    code = replace(code, "bonsai_expand_pq2_byte(code)", "bonsai_expand_pq2_fp8(code)")
    code = replace(code, "int4 dot[4];", f"float4 dot[{fragments}];")
    code = replace(code, "dot[f]=(int4)(0);", "dot[f]=(float4)(0.0f);")
    code = replace(code, "bonsai_mma_i8(dot[f],a,b)", "bonsai_mma_fp8(dot[f],a,b)")
    code = replace(code, """            const int4 d=dot[f];
            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;""",
                   "            result[f]+=dot[f]*scale;")
    assert code.count("f<4;++f") == 4
    return code.replace("f<4;++f", f"f<{fragments};++f")


base, out = map(Path, sys.argv[1:])
source = base.read_text()
experiments = Path(__file__).resolve().parents[2]
fp8_source = (experiments / "EXP-2026-11-25-2056-bonsai-flash-fp8-qk/source/fp8.cl").read_text()
support = fp8_source[:fp8_source.index("__kernel void bonsai_flash_stage_k_fp8")]
start = source.index("__kernel void bonsai_pq2_mmq_int8(")
stop = source.index("\n}\n", start) + 3
out.write_text(source + "\n" + support + "\n" + Path(__file__).with_name("pack.cl").read_text()
               + "\n".join(kernel(source[start:stop], n) for n in (64, 128)))
