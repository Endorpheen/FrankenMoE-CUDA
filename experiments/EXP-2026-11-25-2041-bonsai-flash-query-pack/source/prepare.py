"""Route linear query panels through the existing Bonsai prefill workspace."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "VERIFY_MERGE, KERNEL_COUNT", "VERIFY_MERGE, FLASH_Q_PANEL, FLASH_Q_PACK, KERNEL_COUNT")
text = replace(text, "int decay_small, flash_panel, mmq_cached;", "int decay_small, flash_panel, flash_pack_query, mmq_cached;")
text = replace(text, "id == FLASH_KV_PANEL ? FLASH_PANEL_SHARED : 0",
               "(id == FLASH_KV_PANEL || id == FLASH_Q_PANEL) ? FLASH_PANEL_SHARED : 0")
text = replace(text, "    if (panel) wave &= ~1u;", """    if (panel) wave &= ~1u;
    const int packed_query = panel && m->flash_pack_query;
    if (packed_query) {
        unsigned panel_groups = groups_per_head;
        if (launch(m, FLASH_Q_PACK, 4 * panel_groups, 1, 1, 256,
                   (void *[]){ &q, &m->packed, &count, &panel_groups })) return -1;
        q = m->packed;
        // The shared activation pack arena now owns query panels.
        m->packed_source = m->q8_input = m->ffn_q8_input = 0;
    }""")
text = replace(text, "const enum kernel tile_kind = panel ? FLASH_KV_PANEL",
               "const enum kernel tile_kind = panel ? (packed_query ? FLASH_Q_PANEL : FLASH_KV_PANEL)")
text = replace(text, '    const char *panel = getenv("BONSAI_FLASH_KV_PANEL");', '''
    const char *packed_query = getenv("BONSAI_FLASH_LINEAR_Q");
    m->flash_pack_query = packed_query && strcmp(packed_query, "1") == 0;
    const char *panel = getenv("BONSAI_FLASH_KV_PANEL");''')
text = replace(text, '"bonsai_verify_merge_parallel",',
               '"bonsai_verify_merge_parallel", "bonsai_flash_q_linear_panel", "bonsai_pack_query_panels",')
text = replace(text, """    if (m->gdn_prefill_v32 && check(bonsai_cuda.fn_cuFuncSetAttribute""", """
    if (m->flash_pack_query && check(bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[FLASH_Q_PANEL],
        CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, FLASH_PANEL_SHARED), "packed query panels")) goto error;
    if (m->gdn_prefill_v32 && check(bonsai_cuda.fn_cuFuncSetAttribute""")
output.write_text(text)
