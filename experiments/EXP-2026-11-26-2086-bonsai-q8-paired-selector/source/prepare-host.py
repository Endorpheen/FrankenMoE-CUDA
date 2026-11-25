"""Select exact paired PQ2 selectors for panel-major Bonsai gate/up."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FFN_Q8_PANEL, KERNEL_COUNT",
               "FFN_Q8_PANEL, FFN_Q8_SPREAD, KERNEL_COUNT")
host = replace(host, "ffn_q8, q8_full, q8_pairload, q8_panel;",
               "ffn_q8, q8_full, q8_pairload, q8_panel, q8_spread;")
host = replace(host, '"bonsai_pq2_mmq_int8_full_n128_panel",',
               '"bonsai_pq2_mmq_int8_full_n128_panel", "bonsai_pq2_mmq_int8_full_n128_spread",')
host = replace(host, '    const char *q8_panel = getenv("BONSAI_FFN_Q8_PANEL");',
               '''    const char *spread = getenv("BONSAI_FFN_Q8_SPREAD");
    m->q8_spread = spread && strcmp(spread, "1") == 0;
    const char *q8_panel = getenv("BONSAI_FFN_Q8_PANEL");''')
host = replace(host, "const enum kernel kernel = panel ? FFN_Q8_PANEL : full_q8 ?",
               """const enum kernel kernel = panel ?
                    (m->q8_spread ? FFN_Q8_SPREAD : FFN_Q8_PANEL) : full_q8 ?""")
output.write_text(host)
