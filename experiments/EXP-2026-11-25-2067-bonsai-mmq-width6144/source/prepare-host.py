"""Add the width-6144 specialization without changing the existing choices."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "MMQ_HALF_FULL17408, KERNEL_COUNT",
               "MMQ_HALF_FULL17408, MMQ_HALF_FULL6144, KERNEL_COUNT")
host = replace(host, "mmq_half_acc, mmq_half_full;",
               "mmq_half_acc, mmq_half_full, mmq_half_full6144;")
host = replace(host, '"bonsai_pq2_half_full_17408",',
               '"bonsai_pq2_half_full_17408", "bonsai_pq2_half_full_6144",')
host = replace(host, '    const char *full = getenv("BONSAI_MMQ_HALF_FULL");',
               '''    const char *full6144 = getenv("BONSAI_MMQ_HALF_FULL6144");
    m->mmq_half_full6144 = !full6144 || strcmp(full6144,"0") != 0;
    const char *full = getenv("BONSAI_MMQ_HALF_FULL");''')
host = replace(host, "                if (in_width == 17408) projection = MMQ_HALF_FULL17408;",
               '''                if (in_width == 17408) projection = MMQ_HALF_FULL17408;
                if (in_width == 6144 && m->mmq_half_full6144) projection = MMQ_HALF_FULL6144;''')
out.write_text(host)
