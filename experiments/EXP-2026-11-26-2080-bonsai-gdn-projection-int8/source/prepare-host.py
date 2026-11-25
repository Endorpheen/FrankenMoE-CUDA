"""Route only the two full-tile Bonsai GDN input projections to Q8."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FLASH_FP8_QKPV, KERNEL_COUNT",
               "FLASH_FP8_QKPV, GDN_Q8_GATE, GDN_Q8_QKV, KERNEL_COUNT")
host = replace(host, "ffn_q8, q8_full;", "ffn_q8, q8_full, gdn_q8;")
host = replace(host, '"bonsai_flash_fp8_qkpv",',
               '"bonsai_flash_fp8_qkpv", "bonsai_pq2_mmq_int8_full_n128_6144", '
               '"bonsai_pq2_mmq_int8_full_n128_10240",')
host = replace(host, '    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");',
               '''    const char *gdn_q8 = getenv("BONSAI_GDN_Q8_PREFILL");
    m->gdn_q8 = gdn_q8 && strcmp(gdn_q8, "1") == 0;
    if (m->gdn_q8) fputs("Bonsai prefill: Q8 GDN input projections\\n", stderr);
    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");''')
host = replace(host, '''            if (ffn_gate) ++m->ffn_layer;
            if (m->ffn_q8 && (ffn_gate || ffn_up) && scale_bytes <= m->partial_capacity &&
                (m->ffn_q8 == 1 || m->ffn_layer > (unsigned)(LAYERS - m->ffn_q8))) {''',
               '''            const int gdn_qkv = out_width == 10240 && p[2] == (CUdeviceptr)m->b[3];
            const int gdn_gate = out_width == 6144 && p[2] == (CUdeviceptr)m->b[4];
            const int gdn_q8 = m->gdn_q8 && count % 128 == 0 && in_width == 5120 &&
                p[1] == (CUdeviceptr)m->b[2] && (gdn_qkv || gdn_gate);
            if (ffn_gate) ++m->ffn_layer;
            const int ffn_q8 = m->ffn_q8 && (ffn_gate || ffn_up) &&
                (m->ffn_q8 == 1 || m->ffn_layer > (unsigned)(LAYERS - m->ffn_q8));
            if ((gdn_q8 || ffn_q8) && scale_bytes <= m->partial_capacity) {''')
host = replace(host, '''                const int full_q8 = m->q8_full && count % 128 == 0;
                const unsigned tokens_per_tile = full_q8 ? 128 : 64;
                return launch(m, full_q8 ? FFN_Q8_FULL128 : FFN_Q8_MMQ,''',
               '''                const int full_q8 = m->q8_full && count % 128 == 0;
                const unsigned tokens_per_tile = gdn_q8 || full_q8 ? 128 : 64;
                const enum kernel kernel = gdn_q8 ? (gdn_qkv ? GDN_Q8_QKV : GDN_Q8_GATE) :
                    (full_q8 ? FFN_Q8_FULL128 : FFN_Q8_MMQ);
                return launch(m, kernel,''')
host = replace(host, '                m->packed_source = p[1];',
               '                m->packed_source = p[1];\n                m->ffn_q8_input = 0;')
host = replace(host, '    m->packed_source=(CUdeviceptr)owner;',
               '    m->packed_source=(CUdeviceptr)owner;\n    m->ffn_q8_input = 0;')
host = replace(host, '        case TILES: {',
               '        case TILES: {\n            m->ffn_q8_input = 0;')
output.write_text(host)
