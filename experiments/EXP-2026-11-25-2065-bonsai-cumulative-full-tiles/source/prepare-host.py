"""Select the exact full-tile kernels only on the cumulative Bonsai path."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "MMQ_HALF_ACC32, KERNEL_COUNT",
               "MMQ_HALF_ACC32, MMQ_HALF_FULL5120, MMQ_HALF_FULL17408, KERNEL_COUNT")
host = replace(host, "flash_fused_ordered, mmq_half_acc;", "flash_fused_ordered, mmq_half_acc, mmq_half_full;")
host = replace(host, '"bonsai_pq2_cached_fp16_acc_k32",',
               '"bonsai_pq2_cached_fp16_acc_k32", "bonsai_pq2_half_full_5120", "bonsai_pq2_half_full_17408",')
host = replace(host, '    const char *half_acc = getenv("BONSAI_MMQ_HALF_ACC");',
               '''    const char *full = getenv("BONSAI_MMQ_HALF_FULL");
    m->mmq_half_full = !full || strcmp(full,"0") != 0;
    const char *half_acc = getenv("BONSAI_MMQ_HALF_ACC");''')
host = replace(host, "            const enum kernel projection = partial_half",
               "            enum kernel projection = partial_half")
host = replace(host, "            return launch(m, projection, (out_width+tile-1)/tile,",
               '''            if (partial_half && m->mmq_half_full && out_width % 64 == 0 && count % 128 == 0) {
                if (in_width == 5120) projection = MMQ_HALF_FULL5120;
                if (in_width == 17408) projection = MMQ_HALF_FULL17408;
            }
            return launch(m, projection, (out_width+tile-1)/tile,''')
out.write_text(host)
