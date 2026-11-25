"""Connect fused Q8 activation packing to full Bonsai down projections."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "DECODE_DIRECT_HALF2, KERNEL_COUNT",
               "DECODE_DIRECT_HALF2, FFN_TAIL_Q8_PANEL, FFN_DOWN_Q8_PANEL, KERNEL_COUNT")
host = replace(host, "decode_ordered_merge, decode_half2;",
               "decode_ordered_merge, decode_half2, down_q8;")
host = replace(host, "CUdeviceptr ffn_q8_input;", "CUdeviceptr ffn_q8_input, down_q8_owner;")
host = replace(host, '"bonsai_decode_direct_q4_mma_half2",',
               '"bonsai_decode_direct_q4_mma_half2", "bonsai_ffn_tail_q8_panel", '
               '"bonsai_pq2_mmq_int8_down_n128_spread",')
host = replace(host, '    const char *q8_panel = getenv("BONSAI_FFN_Q8_PANEL");',
               '''    const char *down_q8 = getenv("BONSAI_FFN_DOWN_Q8_FUSED");
    m->down_q8 = down_q8 && !strcmp(down_q8, "1");
    if (m->down_q8) fputs("Bonsai prefill: fused Q8 FFN down\\n", stderr);
    const char *q8_panel = getenv("BONSAI_FFN_Q8_PANEL");''')
host = replace(host, '''    unsigned width=17408;
    if (launch(m,FFN_TAIL,17,m->count,1,256,''',
               '''    unsigned width=17408;
    m->down_q8_owner = 0;
    if (m->down_q8 && owner && m->count % 128 == 0 &&
            (size_t)m->count * (width / 128) * sizeof(float) <= m->partial_capacity) {
        if (launch(m,FFN_TAIL_Q8_PANEL,17,m->count,1,256,
                (void *[]){&gate,&up,&signs,&m->packed,&m->partial,&width})) return -1;
        m->down_q8_owner=(CUdeviceptr)owner;
        m->packed_source=0;
        m->ffn_q8_input=0;
        return 1;
    }
    if (launch(m,FFN_TAIL,17,m->count,1,256,''')
host = replace(host, '            unsigned pairs = count * in_width / 2, tokens = count;',
               '''            unsigned pairs = count * in_width / 2, tokens = count;
            if (m->down_q8_owner && p[1] == m->down_q8_owner) {
                m->down_q8_owner = 0;
                if (in_width != 17408 || out_width != 5120 || count % 128 != 0 ||
                    p[2] != (CUdeviceptr)m->b[18]) return -1;
                return launch(m, FFN_DOWN_Q8_PANEL, out_width/64, count/128, 1, 256,
                    (void *[]){ &p[0], &m->packed, &m->partial, &p[2], &in_width, &out_width, &tokens });
            }''')
host = replace(host, "    m->ffn_layer = 0;", "    m->ffn_layer = 0;\n    m->down_q8_owner = 0;")
output.write_text(host)
