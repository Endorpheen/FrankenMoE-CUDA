"""Add diagnostic integer gate/up paths, preserving other projections."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text()


def replace(old, new):
    global text
    assert text.count(old) == 1, old
    text = text.replace(old, new)


replace("MATRIX_ROWS2, KERNEL_COUNT", "MATRIX_ROWS2, Q8_PACK, RESIDUAL_PACK, Q8_MMQ, RESIDUAL_MMQ, KERNEL_COUNT")
replace("    int grouped_decode, matvec_rows2;", """    int grouped_decode, matvec_rows2;
    unsigned ffn_quant_mode, ffn_layers_seen;
    CUdeviceptr quantized_source;""")
replace("            m->packed_source = 0;\n            return launch(m, ROTATE",
        "            m->packed_source = 0;\n            m->quantized_source = 0;\n            return launch(m, ROTATE")
replace("            unsigned pairs = count * in_width / 2, tokens = count;",
        """            unsigned pairs = count * in_width / 2, tokens = count;
            if (m->ffn_quant_mode && count >= 128 && in_width == 5120 && out_width == 17408 &&
                (p[2] == (CUdeviceptr)m->b[14] || p[2] == (CUdeviceptr)m->b[15])) {
                if (p[2] == (CUdeviceptr)m->b[14]) ++m->ffn_layers_seen;
                if (m->ffn_quant_mode != 3 || m->ffn_layers_seen > 48) {
                    const int residual = m->ffn_quant_mode != 1;
                    unsigned groups = count * (in_width / 128);
                    CUdeviceptr scales = m->partial;
                    CUdeviceptr correction = m->packed + (size_t)m->ubatch * 17408;
                    if (m->quantized_source != p[1]) {
                        const int rc = residual
                            ? launch(m, RESIDUAL_PACK, (groups+3)/4, 1, 1, 128,
                                     (void *[]){ &p[1], &m->packed, &correction, &scales, &groups })
                            : launch(m, Q8_PACK, (groups+3)/4, 1, 1, 128,
                                     (void *[]){ &p[1], &m->packed, &scales, &groups });
                        if (rc) return -1;
                        m->quantized_source = p[1];
                        m->packed_source = 0;
                    }
                    return residual
                        ? launch(m, RESIDUAL_MMQ, (out_width+63)/64, (count+63)/64, 1, 256,
                                 (void *[]){ &p[0], &m->packed, &correction, &scales, &p[2],
                                             &in_width, &out_width, &tokens })
                        : launch(m, Q8_MMQ, (out_width+63)/64, (count+63)/64, 1, 256,
                                 (void *[]){ &p[0], &m->packed, &scales, &p[2],
                                             &in_width, &out_width, &tokens });
                }
            }""")
replace("    CUdevice device;", """    const char *quant = getenv("BONSAI_FFN_QUANT");
    if (quant && strcmp(quant, "off") != 0) {
        if (strcmp(quant, "plain") == 0) m->ffn_quant_mode = 1;
        else if (strcmp(quant, "residual") == 0) m->ffn_quant_mode = 2;
        else if (strcmp(quant, "last16") == 0) m->ffn_quant_mode = 3;
        else goto error;
        fprintf(stderr, "Diagnostic FFN gate/up: %s\\n", quant);
    }
    CUdevice device;""")
replace('"bonsai_pq2_matvec_rows2",', '''"bonsai_pq2_matvec_rows2",
        "bonsai_pack_q8_128", "bonsai_pack_q8_residual", "bonsai_pq2_mmq_int8", "bonsai_pq2_mmq_q8_residual",''')
replace("    m->packed_source = 0;\n    int32_t positions", """    m->packed_source = 0;
    m->quantized_source = 0;
    m->ffn_layers_seen = 0;
    int32_t positions""")
output.write_text(text)
