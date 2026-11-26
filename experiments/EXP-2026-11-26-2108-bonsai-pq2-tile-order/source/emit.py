"""Generate alternate CTA orders for the exact Bonsai PQ2 prefill kernel."""

from pathlib import Path
import re
import sys


ENTRY = "bonsai_pq2_mmq_int8_full_n128_spread"
GROUPS = (2, 4, 8)
ROW_TILES = 272
TOKEN_TILES = 8


def variants(source):
    start = source.index(f"__kernel void {ENTRY}(")
    end = source.index("\n}", start) + len("\n}")
    kernel = source[start:end]
    anchor = "row_base=64*work_group(),panel=flash_grid_y(),token_base=128*panel;"
    assert kernel.count(anchor) == 1
    for size in GROUPS:
        mapping = {
            (x // size + (y % size) * (ROW_TILES // size),
             x % size + (y // size) * size)
            for x in range(ROW_TILES) for y in range(TOKEN_TILES)
        }
        assert mapping == {(x, y) for x in range(ROW_TILES) for y in range(TOKEN_TILES)}
        replacement = f"""row_base=64*(work_group()/{size}+(flash_grid_y()%{size})*{ROW_TILES//size});
    const uint panel=work_group()%{size}+(flash_grid_y()/{size})*{size};
    const uint token_base=128*panel;"""
        yield kernel.replace(ENTRY, f"{ENTRY}_order{size}", 1).replace(anchor, replacement)


def extract(source):
    header = [re.search(rf"^\.{key}[^\n]*", source, re.M).group()
              for key in ("version", "target", "address_size")]
    for size in GROUPS:
        start = source.index(f".visible .entry {ENTRY}_order{size}(")
        end = source.index("\n}", start) + len("\n}")
        header.append(source[start:end])
    return "\n\n".join(header) + "\n"


if __name__ == "__main__":
    args = sys.argv[1:]
    extracting = args[0] == "--extract"
    ref, out = map(Path, args[extracting:])
    source = ref.read_text()
    out.write_text(extract(source) if extracting else source + "\n" + "\n".join(variants(source)) + "\n")
