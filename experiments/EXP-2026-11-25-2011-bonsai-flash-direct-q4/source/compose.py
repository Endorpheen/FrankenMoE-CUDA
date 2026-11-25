"""Add exact Q reuse to the EXP-2006 Bonsai HTTP candidate."""
from pathlib import Path
import sys

base, dst = map(Path, sys.argv[1:])
text = base.read_text()


def swap(old, new):
    global text
    assert text.count(old) == 1, old
    text = text.replace(old, new)


swap("MATRIX_ROWS2, KERNEL_COUNT", "MATRIX_ROWS2, FLASH_GROUP2, FLASH_GROUP4, KERNEL_COUNT")
swap("    int grouped_decode, matvec_rows2;", "    int grouped_decode, matvec_rows2, flash_grouped;")
swap("    const enum kernel tile_kind = compact",
     """    const unsigned grouping = compact && m->async_fragments && m->flash_grouped ?
        (keys >= 16384 ? 4 : 2) : 1;
    const enum kernel tile_kind = grouping == 4 ? FLASH_GROUP4 : grouping == 2 ? FLASH_GROUP2 : compact""")
swap("launch(m, tile_kind, active, tiles, 1, 128,", "launch(m, tile_kind, active, (tiles+grouping-1)/grouping, 1, 128,")
swap('    if (m->async_fragments) fputs("Flash fragments: async staging and ldmatrix\\n", stderr);',
     '''    if (m->async_fragments) fputs("Flash fragments: async staging and ldmatrix\\n", stderr);
    const char *reuse = getenv("BONSAI_FLASH_TILE_REUSE");
    m->flash_grouped = !reuse || strcmp(reuse, "1") == 0;
    if (m->async_fragments && m->flash_grouped) fputs("Flash: reuse Q across KV tiles\\n", stderr);''')
swap('"bonsai_pq2_matvec_rows2",', '"bonsai_pq2_matvec_rows2", "bonsai_flash_tile_group2", "bonsai_flash_tile_group4",')
dst.write_text(text)
