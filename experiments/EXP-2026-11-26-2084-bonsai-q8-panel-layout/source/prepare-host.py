"""Select matching packed activation layout and MMQ for full Bonsai gate/up tiles."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FFN_Q8_PAIRLOAD, KERNEL_COUNT",
               "FFN_Q8_PAIRLOAD, FFN_Q8_PANEL_PACK, FFN_Q8_PANEL, KERNEL_COUNT")
host = replace(host, "ffn_q8, q8_full, q8_pairload;", "ffn_q8, q8_full, q8_pairload, q8_panel;")
host = replace(host, '"bonsai_pq2_mmq_int8_full_n128_pairload",',
               '"bonsai_pq2_mmq_int8_full_n128_pairload", "bonsai_pack_q8_panel128", '
               '"bonsai_pq2_mmq_int8_full_n128_panel",')
host = replace(host, '    const char *pairload = getenv("BONSAI_FFN_Q8_PAIRLOAD");',
               '''    const char *q8_panel = getenv("BONSAI_FFN_Q8_PANEL");
    m->q8_panel = q8_panel && strcmp(q8_panel, "1") == 0;
    if (m->q8_panel) fputs("Bonsai prefill: panel-major Q8 activations\\n", stderr);
    const char *pairload = getenv("BONSAI_FFN_Q8_PAIRLOAD");''')
host = replace(host, '''                unsigned groups = count * (in_width / 128);
                if (m->ffn_q8_input != p[1]) {
                    if (launch(m, FFN_Q8_PACK,''',
               '''                const int full_q8 = m->q8_full && count % 128 == 0;
                const int panel = full_q8 && m->q8_pairload && m->q8_panel;
                unsigned groups = count * (in_width / 128);
                if (m->ffn_q8_input != p[1]) {
                    if (launch(m, panel ? FFN_Q8_PANEL_PACK : FFN_Q8_PACK,''')
host = replace(host, '''                const int full_q8 = m->q8_full && count % 128 == 0;
                const unsigned tokens_per_tile = full_q8 ? 128 : 64;
                const enum kernel kernel = full_q8 ?
                    (m->q8_pairload ? FFN_Q8_PAIRLOAD : FFN_Q8_FULL128) : FFN_Q8_MMQ;''',
               '''                const unsigned tokens_per_tile = full_q8 ? 128 : 64;
                const enum kernel kernel = panel ? FFN_Q8_PANEL : full_q8 ?
                    (m->q8_pairload ? FFN_Q8_PAIRLOAD : FFN_Q8_FULL128) : FFN_Q8_MMQ;''')
output.write_text(host)
