"""Use the exact ordered verifier merge for long Bonsai single-query attention."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "DECODE_DIRECT_MMA, KERNEL_COUNT",
               "DECODE_DIRECT_MMA, DECODE_ORDERED_MERGE, KERNEL_COUNT")
host = replace(host, "q8_spread, decode_mma;", "q8_spread, decode_mma, decode_ordered_merge;")
host = replace(host, '"bonsai_decode_direct_q4_mma",',
               '"bonsai_decode_direct_q4_mma", "bonsai_verify_merge_parallel",')
host = replace(host, '    const char *mma = getenv("BONSAI_DECODE_DIRECT_MMA");',
               '''    const char *decode_merge = getenv("BONSAI_DECODE_ORDERED_MERGE");
    m->decode_ordered_merge = decode_merge && !strcmp(decode_merge, "1");
    const char *mma = getenv("BONSAI_DECODE_DIRECT_MMA");''')
host = replace(host, '''                return launch(m, MERGE, tasks, 1, 1, 32,
                    (void *[]){ &m->partial, &p[3], &m->attention, &start });''',
               '''                if (m->decode_ordered_merge && keys >= 32768) {
                    return launch(m, DECODE_ORDERED_MERGE, tasks, 1, 1, 128,
                        (void *[]){ &m->partial, &p[3], &keys, &n_tiles });
                }
                return launch(m, MERGE, tasks, 1, 1, 32,
                    (void *[]){ &m->partial, &p[3], &m->attention, &start });''')
output.write_text(host)
