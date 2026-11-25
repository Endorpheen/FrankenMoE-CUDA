"""Use half2 unpack only for full cumulative FFN down tiles."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FLASH_FP8_QK, KERNEL_COUNT",
               "FLASH_FP8_QK, MMQ_HALF_FULL_DOWN_HALF2, KERNEL_COUNT")
host = replace(host, "mmq_half_full6144, flash_fp8_qk;",
               "mmq_half_full6144, flash_fp8_qk, mmq_half2_down;")
host = replace(host, '"bonsai_flash_fp8_qk",',
               '"bonsai_flash_fp8_qk", "bonsai_pq2_half_unpack_half2_17408",')
host = replace(host, '    const char *full6144 = getenv("BONSAI_MMQ_HALF_FULL6144");',
               '''    const char *half2_down = getenv("BONSAI_MMQ_HALF2_DOWN");
    m->mmq_half2_down = !half2_down || strcmp(half2_down,"0") != 0;
    const char *full6144 = getenv("BONSAI_MMQ_HALF_FULL6144");''')
host = replace(host, "                if (in_width == 17408) projection = MMQ_HALF_FULL17408;",
               '''                if (in_width == 17408) {
                    projection = m->mmq_half2_down && out_width == 5120 &&
                        p[2] == (CUdeviceptr)m->b[18] ? MMQ_HALF_FULL_DOWN_HALF2 : MMQ_HALF_FULL17408;
                }''')
out.write_text(host)
