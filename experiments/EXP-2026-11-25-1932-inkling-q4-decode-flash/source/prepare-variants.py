from pathlib import Path

source = Path("../inkling-decode-warp-softmax/flash-mma.cl").read_text()
start = source.index("__kernel void goose_i2_flash_mma_decode(")
body = source[start:]
prefix = source[:start]
specialized = body.replace("uint query_tile, head, part, thread;", "uint head, part, thread;")
specialized = specialized.replace('    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(query_tile));\n', "")
specialized = specialized.replace("    const int64_t query = 16 * (int64_t)query_tile;", "    const int64_t query = 0;")
specialized = specialized.replace("    const int tail = (int)(queries - query < 16 ? queries - query : 16);", "    const int tail = 1;")
specialized = specialized.replace("    q4 &= 1;", "    q4 = 1;")
ordered_sum = "                for (int i = 0; i < 32; ++i) total += scores[i];"
qk_loop = "        #pragma unroll\n        for (int dim="
pv_loop = "        #pragma unroll\n        for (int j="
assert specialized.count(ordered_sum) == 1
assert specialized.count(qk_loop) == specialized.count(pv_loop) == 1
variants = {
    "sum": body.replace(ordered_sum, "                #pragma unroll 1\n" + ordered_sum),
    "shape": specialized,
    "shape-sum": specialized.replace(ordered_sum, "                #pragma unroll 1\n" + ordered_sum),
    "shape-qk": specialized.replace(qk_loop, qk_loop.replace("#pragma unroll", "#pragma unroll 1")),
    "shape-pv": specialized.replace(pv_loop, pv_loop.replace("#pragma unroll", "#pragma unroll 1")),
}
for name, content in variants.items():
    Path(f"{name}.cl").write_text(prefix + content)
