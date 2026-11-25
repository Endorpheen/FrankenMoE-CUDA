"""Generate cached PQ2 K64 tiles with asynchronous activation lookahead."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


def variant(source, width):
    name = f"bonsai_pq2_cached_64x{width}_k64"
    start = source.index(f"__kernel void {name}(")
    end = source.index("\n}\n", start) + 3
    code = source[start:end].replace(name, f"bonsai_pq2_cached_pipeline_n{width}")
    code = replace(code, f"uint x[{width}][36];", f"uint x[2][{width}][36];")
    begin = code.index(f"        for (uint packed=thread*4;packed<{width}*32;packed+=1024)")
    finish = code.index("        #pragma unroll\n        for (uint lane_row", begin)
    copy = code[begin:finish]
    first = copy.replace("step*32", "0").replace("&x[", "&x[0][", 1)
    first = first + '        __asm__ volatile("cp.async.commit_group;" ::: "memory");\n'
    next_ = copy.replace("step*32", "(step+1)*32").replace("&x[", "&x[(step+1)&1][", 1)
    next_ = next_ + '        __asm__ volatile("cp.async.commit_group;" ::: "memory");\n'
    code = code[:begin] + code[finish:]
    anchor = "    for (uint step=0;step<2*blocks;++step) {"
    code = replace(code, anchor, """    if (blocks) {
""" + first + "    }\n" + anchor + """
        const int next = step+1 < 2*blocks;
        if (next) {
""" + "\n".join("    " + line for line in next_.splitlines()) + "\n        }")
    code = replace(code, "        bonsai_copy_complete();", """        if (next) {
            __asm__ volatile("cp.async.wait_group 1;" ::: "memory");
        } else {
            __asm__ volatile("cp.async.wait_group 0;" ::: "memory");
        }""")
    return replace(code, "&x[warp_token+8*i][0]", "&x[step&1][warp_token+8*i][0]")


if __name__ == "__main__":
    base, out = map(Path, sys.argv[1:])
    source = base.read_text()
    out.write_text(source + "\n" + "\n".join(variant(source, width) for width in (64, 128)))
