"""Route long single-query Bonsai attention to the direct Q4 MMA candidate."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FFN_Q8_SPREAD, KERNEL_COUNT",
               "FFN_Q8_SPREAD, DECODE_DIRECT_MMA, KERNEL_COUNT")
host = replace(host, "q8_panel, q8_spread;", "q8_panel, q8_spread, decode_mma;")
host = replace(host, '"bonsai_pq2_mmq_int8_full_n128_spread",',
               '"bonsai_pq2_mmq_int8_full_n128_spread", "bonsai_decode_direct_q4_mma",')
host = replace(host, '    const char *grouped = getenv("BONSAI_DECODE_GQA");',
               '''    const char *mma = getenv("BONSAI_DECODE_DIRECT_MMA");
    m->decode_mma = !mma ? 0 : !strcmp(mma, "1") ? 1 : !strcmp(mma, "diagnostic") ? 2 : 0;
    if (m->decode_mma) fputs("Bonsai single-query attention: direct Q4 MMA\\n", stderr);
    const char *grouped = getenv("BONSAI_DECODE_GQA");''')
host = replace(host, '''                if (launch(m, heads == 3 ? DECODE_GQA3 : DECODE_GQA2, 24 / heads, tiles, 1, 32,
                    (void *[]){ &p[0], &p[1], &p[2], &m->partial, &keys, &n_tiles, &scale })) return -1;''',
               '''                if (m->decode_mma && (keys >= 32768 || m->decode_mma == 2)) {
                    if (launch(m, DECODE_DIRECT_MMA, 4, tiles, 1, 128,
                        (void *[]){ &p[0], &p[1], &p[2], &m->partial, &keys, &n_tiles })) return -1;
                } else if (launch(m, heads == 3 ? DECODE_GQA3 : DECODE_GQA2, 24 / heads, tiles, 1, 32,
                    (void *[]){ &p[0], &p[1], &p[2], &m->partial, &keys, &n_tiles, &scale })) return -1;''')
output.write_text(host)
