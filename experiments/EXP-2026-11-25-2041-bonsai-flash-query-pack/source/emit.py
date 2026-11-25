"""Generate two ways to consume the prepacked FP16 query from the same Flash tile."""
from pathlib import Path
import re
import sys


base, output = map(Path, sys.argv[1:])
text = base.read_text()
start = text.index("__kernel void bonsai_flash_kv_lean2(")
end = text.index("\n__kernel", start + 1)
control = text[start:end]
common = control.replace("__global const float *query", "__global const uint *query")
start = common.index("    for (uint index = thread; index < FLASH_ROWS * 128;")
end = common.index("\n    for (uint j = 0;", start)
staged = common[:start] + """    for (uint index = thread * 4; index < FLASH_ROWS * 128; index += 512) {
        const uint r = index / 128, col = index % 128;
        const uint original = first_row + r, token = original / 6, h = original % 6;
        const uint valid = token < queries ? 16 : 0;
        const size_t source = valid ? ((size_t)token * 24 + kv_head * 6 + h) * 128 + col : 0;
        bonsai_copy16(&query_half[r][col], query + source, valid);
    }
    bonsai_copy_complete();
    sync_threads();
""" + common[end:]
staged = staged.replace("bonsai_flash_kv_lean2", "bonsai_flash_q_packed_panel")
direct = common[:start] + common[end:]
direct = direct.replace("bonsai_flash_kv_lean2", "bonsai_flash_q_direct_panel")
direct = direct.replace("panel_scratch[12544]", "panel_scratch[8320]")
direct = re.sub(r"^    __local uint \(\*query_half\).*\n", "", direct, count=1, flags=re.M)
direct = direct.replace("panel_scratch + 4224 + panel * 2048", "panel_scratch + panel * 2048")
direct = direct.replace("panel_scratch + 8320", "panel_scratch + 4096")
anchor = "                const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);"
assert direct.count(anchor) == 1
direct = direct.replace(anchor, """                const uint r0 = first_row + lane_row, r1 = r0 + 8;
                const uint p = k + lane_pair;
                const uint4 a = (uint4)(
                    bonsai_packed_query(qbase, r0, queries, p),
                    bonsai_packed_query(qbase, r1, queries, p),
                    bonsai_packed_query(qbase, r0, queries, p + 4),
                    bonsai_packed_query(qbase, r1, queries, p + 4));""")
direct = direct.replace("    const float neg_inf", "    const __global uint *qbase = query + kv_head * 6 * 128;\n    const float neg_inf")
helper = """static inline uint bonsai_packed_query(__global const uint *query, uint row,
                                       uint queries, uint pair) {
    const uint token = row / 6, head = row % 6;
    return token < queries ? query[((size_t)token * 24 + head) * 128 + pair] : 0;
}
"""
panels = staged.replace("bonsai_flash_q_packed_panel", "bonsai_flash_q_linear_panel")
start = panels.index("    for (uint index = thread * 4;")
end = panels.index("    bonsai_copy_complete();", start)
panels = panels[:start] + """    const __global uint *query_panel = query + (size_t)work * FLASH_ROWS * 128;
    for (uint index = thread * 4; index < FLASH_ROWS * 128; index += 512) {
        bonsai_copy16(&query_half[index / 128][index % 128], query_panel + index, 16);
    }
""" + panels[end:]
output.write_text(staged + "\n" + helper + direct + "\n" + panels)
