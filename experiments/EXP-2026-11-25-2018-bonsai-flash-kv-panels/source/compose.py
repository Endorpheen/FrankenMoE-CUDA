"""Dispatch paired K/V staging only when compact group alignment is exact."""
from pathlib import Path
import sys

source,out=map(Path,sys.argv[1:])
s=source.read_text()


def replace(a,b):
    global s
    assert s.count(a)==1,a
    s=s.replace(a,b)


replace("DECAY_WARPPACK, KERNEL_COUNT","DECAY_WARPPACK, FLASH_KV_PANEL, KERNEL_COUNT")
replace("CONTEXT_MIN = 1, UBATCH_MAX = 1024,", "CONTEXT_MIN = 1, UBATCH_MAX = 1024, FLASH_PANEL_SHARED = 50176,")
replace("    int decay_small;","    int decay_small, flash_panel;")
replace("    return check(bonsai_cuda.fn_cuLaunchKernel(m->kernel[id], x, y, z, threads, 1, 1, 0, NULL, args, NULL),",
        """    const unsigned shared = id == FLASH_KV_PANEL ? FLASH_PANEL_SHARED : 0;
    return check(bonsai_cuda.fn_cuLaunchKernel(m->kernel[id], x, y, z, threads, 1, 1, shared, NULL, args, NULL),""")
replace("const unsigned wave = (unsigned)(m->partial_capacity / group_bytes);",
        "unsigned wave = (unsigned)(m->partial_capacity / group_bytes);")
replace("    const unsigned grouping = compact && m->async_fragments && m->flash_grouped ?\n        (keys >= 16384 ? 4 : 2) : 1;",
        """    const unsigned groups_per_head = compact ? (count*6+15)/16 : (count+1)/2;
    const int panel = compact && m->async_fragments && m->flash_grouped && m->flash_panel &&
        keys >= 8192 && wave >= 2 && groups_per_head % 2 == 0;
    if (panel) wave &= ~1u;
    const unsigned grouping = panel ? 2 : compact && m->async_fragments && m->flash_grouped ?
        (keys >= 16384 ? 4 : 2) : 1;""")
replace("    const enum kernel tile_kind = grouping == 4", "    const enum kernel tile_kind = panel ? FLASH_KV_PANEL : grouping == 4")
replace("    const unsigned groups = 4*(compact ? (count*6+15)/16 : (count+1)/2);",
        "    const unsigned groups = 4*groups_per_head;")
replace("launch(m, tile_kind, active, (tiles+grouping-1)/grouping, 1, 128,",
        "launch(m, tile_kind, panel ? active/2 : active, (tiles+grouping-1)/grouping, 1, panel ? 256 : 128,")
replace('    const char *reuse = getenv("BONSAI_FLASH_TILE_REUSE");',
        '''    const char *panel = getenv("BONSAI_FLASH_KV_PANEL");
    m->flash_panel = !panel || strcmp(panel, "1") == 0;
    const char *reuse = getenv("BONSAI_FLASH_TILE_REUSE");''')
replace('"bonsai_decay_warp_pack",','"bonsai_decay_warp_pack", "bonsai_flash_kv_lean2",')
replace("    if (m->gdn_prefill_v32 && check(bonsai_cuda.fn_cuFuncSetAttribute",
        """    if (m->flash_panel) {
        m->flash_panel = bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[FLASH_KV_PANEL],
            CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, FLASH_PANEL_SHARED) == CUDA_SUCCESS;
        if (m->flash_panel) fputs("Bonsai Flash: shared K/V query panels\\n",stderr);
    }
    if (m->gdn_prefill_v32 && check(bonsai_cuda.fn_cuFuncSetAttribute""")
out.write_text(s)
