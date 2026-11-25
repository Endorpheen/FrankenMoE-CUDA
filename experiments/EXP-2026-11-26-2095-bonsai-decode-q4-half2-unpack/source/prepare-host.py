"""Select bitwise half2 Q4 unpack in the long direct MMA path."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "DECODE_ORDERED_MERGE, KERNEL_COUNT",
               "DECODE_ORDERED_MERGE, DECODE_DIRECT_HALF2, KERNEL_COUNT")
host = replace(host, "decode_mma, decode_ordered_merge;",
               "decode_mma, decode_ordered_merge, decode_half2;")
host = replace(host, '"bonsai_verify_merge_parallel",',
               '"bonsai_verify_merge_parallel", "bonsai_decode_direct_q4_mma_half2",')
host = replace(host, '    const char *mma = getenv("BONSAI_DECODE_DIRECT_MMA");',
               '''    const char *decode_half2 = getenv("BONSAI_DECODE_DIRECT_HALF2");
    m->decode_half2 = decode_half2 && !strcmp(decode_half2, "1");
    const char *mma = getenv("BONSAI_DECODE_DIRECT_MMA");''')
host = replace(host, '                    if (launch(m, DECODE_DIRECT_MMA, 4, tiles, 1, 128,',
               '''                    const enum kernel tile = m->decode_half2 ?
                        DECODE_DIRECT_HALF2 : DECODE_DIRECT_MMA;
                    if (launch(m, tile, 4, tiles, 1, 128,''')
output.write_text(host)
