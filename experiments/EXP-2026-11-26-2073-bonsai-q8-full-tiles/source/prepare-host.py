"""Use the exact N128 tile inside the existing selective Q8 dispatch."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "MMQ_HALF_FULL_DOWN_HALF2, KERNEL_COUNT",
               "MMQ_HALF_FULL_DOWN_HALF2, FFN_Q8_FULL128, KERNEL_COUNT")
host = replace(host, "norm_wide, ffn_q8;", "norm_wide, ffn_q8, q8_full;")
host = replace(host, '"bonsai_pq2_half_unpack_half2_17408",',
               '"bonsai_pq2_half_unpack_half2_17408", "bonsai_pq2_mmq_int8_full_n128",')
host = replace(host, '    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");',
               '''    const char *full_q8 = getenv("BONSAI_FFN_Q8_FULL128");
    m->q8_full = !full_q8 || strcmp(full_q8,"0") != 0;
    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");''')
host = replace(host, "                return launch(m, FFN_Q8_MMQ, (out_width + 63) / 64, (count + 63) / 64, 1, 256,",
               '''                const int full_q8 = m->q8_full && count % 128 == 0;
                const unsigned tokens_per_tile = full_q8 ? 128 : 64;
                return launch(m, full_q8 ? FFN_Q8_FULL128 : FFN_Q8_MMQ,
                    (out_width+63)/64, (count+tokens_per_tile-1)/tokens_per_tile, 1, 256,''')
out.write_text(host)
