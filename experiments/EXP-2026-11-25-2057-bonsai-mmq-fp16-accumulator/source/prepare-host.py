"""Wire half-precision K32 partials into the isolated Bonsai test host."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FLASH_FUSED_ORDERED, KERNEL_COUNT",
               "FLASH_FUSED_ORDERED, MMQ_HALF_ACC32, KERNEL_COUNT")
host = replace(host, "mmq_cached, flash_fused_ordered;", "mmq_cached, flash_fused_ordered, mmq_half_acc;")
host = replace(host, '"bonsai_flash_kv_fused_ordered",',
               '"bonsai_flash_kv_fused_ordered", "bonsai_pq2_cached_fp16_acc_k32",')
host = replace(host, '    const char *cached = getenv("BONSAI_MMQ_CACHED_PQ2");',
               '''    const char *half_acc = getenv("BONSAI_MMQ_HALF_ACC");
    m->mmq_half_acc = half_acc && strcmp(half_acc,"1") == 0;
    const char *cached = getenv("BONSAI_MMQ_CACHED_PQ2");''')
host = replace(host, "            const unsigned tokens_per_tile = k64 && count >= 512 ? 128 : tile;",
               '''            const int partial_half = k64 && m->mmq_cached && m->mmq_half_acc;
            const unsigned tokens_per_tile = k64 && (count >= 512 || partial_half) ? 128 : tile;''')
host = replace(host, "            const enum kernel projection = k64",
               "            const enum kernel projection = partial_half ? MMQ_HALF_ACC32 : k64")
out.write_text(host)
