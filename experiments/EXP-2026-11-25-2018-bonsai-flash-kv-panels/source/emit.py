"""Share K/V staging between same-head panels with unchanged MMA and merge."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
for tile_group in (2,4):
    name = f"bonsai_flash_tile_group{tile_group}"
    begin = source.index(f"__kernel void {name}(")
    end = source.index("\n}\n",begin)+3
    code=source[begin:end]

    def replace(old,new):
        global code
        assert code.count(old)==1,old
        code=code.replace(old,new)

    replace(name,f"bonsai_flash_kv_panel{tile_group}")
    replace("""    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][FLASH_KEYS/2];""",
            """    __local __attribute__((aligned(16))) uint panel_scratch[15488];
    const uint physical_thread = local_id();
    const uint panel = physical_thread / 128;
    __local uint (*query_half)[132] = (__local uint (*)[132])(panel_scratch + panel * 2112);
    __local float (*score)[FLASH_KEYS] = (__local float (*)[FLASH_KEYS])(panel_scratch + 4224 + panel * 2048);
    __local uint (*probability)[FLASH_KEYS/2] = (__local uint (*)[FLASH_KEYS/2])(panel_scratch + 8320 + panel * 1024);
    __local uint *staging = panel_scratch + 10368;""")
    replace("    const uint work = work_group() + group_start;",
            """    const uint group = work_group() * 2 + panel;
    const uint work = group + group_start;""")
    replace("    const uint tile_base = flash_grid_y()",
            """    const uint pair_last = flash_min(first_row+(2-panel)*FLASH_ROWS, queries*6)-1;
    const uint tile_base = flash_grid_y()""")
    replace("if (tile_base * FLASH_KEYS >= first_visible + last_row/6)",
            "if (tile_base * FLASH_KEYS >= first_visible + pair_last/6)")
    replace("const uint thread = local_id(), warp", "const uint thread = physical_thread % 128, warp")
    replace("if (tile >= tile_count || key_start >= first_visible + last_row/6) break;",
            """if (tile >= tile_count || key_start >= first_visible + pair_last/6) break;
        const int live = key_start < first_visible + last_row/6;""")
    replace("((ulong)work_group()*tile_count + tile)", "((ulong)group*tile_count + tile)")
    assert code.count("uint index = thread*4; index < 32*128; index += 512")==2
    code=code.replace("uint index = thread*4; index < 32*128; index += 512",
                      "uint index = physical_thread*4; index < 32*128; index += 1024")
    replace("            if (lane == 0) {", "            if (lane == 0 && live) {")
    replace("        for (uint result = 0; result < 8; ++result) {\n            const uint column",
            "        for (uint result = 0; result < (live ? 8u : 0u); ++result) {\n            const uint column")
    print(code)
    replace(f"bonsai_flash_kv_panel{tile_group}",f"bonsai_flash_kv_lean{tile_group}")
    replace("panel_scratch[15488]","panel_scratch[12544]")
    replace("    __local float (*score)[FLASH_KEYS] = (__local float (*)[FLASH_KEYS])",
            "    __local uint (*score)[FLASH_KEYS] = (__local uint (*)[FLASH_KEYS])")
    replace("    __local uint (*probability)[FLASH_KEYS/2] = (__local uint (*)[FLASH_KEYS/2])(panel_scratch + 8320 + panel * 1024);",
            "    __local uint (*probability)[FLASH_KEYS] = score;")
    replace("staging = panel_scratch + 10368","staging = panel_scratch + 8320")
    for fragment in ("x","y","z","w"):
        replace(f"= acc.{fragment} * 0.0625f;",f"= as_uint(acc.{fragment} * 0.0625f);")
    assert code.count("score[r][z]")==2
    code=code.replace("score[r][z]","as_float(score[r][z])").replace("score[r][z+1]","as_float(score[r][z+1])")
    replace("            float sum = 0.0f;","            float sum = 0.0f;\n            uint probabilities[2];")
    replace("                probability[r][z/2] = bonsai_half_pair(lo,hi);",
            "                probabilities[z/64] = bonsai_half_pair(lo,hi);")
    replace("            if (lane == 0 && live) {",
            """            __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory");
            probability[r][lane] = probabilities[0];
            probability[r][lane+32] = probabilities[1];
            if (lane == 0 && live) {""")
    replace("&probability[0][0],FLASH_KEYS/2","&probability[0][0],FLASH_KEYS")
    replace("(index/16)*20+index%16","(index/16)*16+index%16")
    replace("(32*result + 8*warp)*20","(32*result + 8*warp)*16")
    replace("staging+feature_block,20,ksub","staging+feature_block,16,ksub")
    print(code)
