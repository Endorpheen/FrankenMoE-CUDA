"""Reuse the validated PQ2 Q8 kernels for Bonsai prefill gate/up."""
from pathlib import Path
import sys

path, out = map(Path, sys.argv[1:])
src = path.read_text()


def change(a, b):
    global src
    assert src.count(a) == 1, a
    src = src.replace(a, b)


change("NORM_WIDE_LARGE, KERNEL_COUNT", "NORM_WIDE_LARGE, FFN_Q8_PACK, FFN_Q8_MMQ, KERNEL_COUNT")
change("    int q8_mode, in_decode, norm_wide;",
       "    int q8_mode, in_decode, norm_wide, ffn_q8;\n    CUdeviceptr ffn_q8_input;\n    unsigned ffn_layer;")
change("            m->packed_source = 0;\n            m->q8_input = 0;",
       "            m->packed_source = 0;\n            m->q8_input = 0;\n            m->ffn_q8_input = 0;")
change("            unsigned pairs = count * in_width / 2, tokens = count;",
       """            unsigned pairs = count * in_width / 2, tokens = count;
            const size_t scale_bytes = (size_t)count * (in_width / 128) * sizeof(float);
            const int ffn_gate = count >= 128 && in_width == 5120 && out_width == 17408 &&
                p[2] == (CUdeviceptr)m->b[14];
            const int ffn_up = count >= 128 && in_width == 5120 && out_width == 17408 &&
                p[2] == (CUdeviceptr)m->b[15];
            if (ffn_gate) ++m->ffn_layer;
            if (m->ffn_q8 && (ffn_gate || ffn_up) && scale_bytes <= m->partial_capacity &&
                (m->ffn_q8 == 1 || m->ffn_layer > (unsigned)(LAYERS - m->ffn_q8))) {
                unsigned groups = count * (in_width / 128);
                if (m->ffn_q8_input != p[1]) {
                    if (launch(m, FFN_Q8_PACK, (groups + 3) / 4, 1, 1, 128,
                        (void *[]){ &p[1], &m->packed, &m->partial, &groups })) return -1;
                    m->ffn_q8_input = p[1];
                    m->packed_source = 0;
                }
                return launch(m, FFN_Q8_MMQ, (out_width + 63) / 64, (count + 63) / 64, 1, 256,
                    (void *[]){ &p[0], &m->packed, &m->partial, &p[2], &in_width, &out_width, &tokens });
            }""")
change("    CUdevice device;", """    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");
    m->ffn_q8 = 16;
    if (ffn_q8) {
        if (strcmp(ffn_q8, "0") == 0) m->ffn_q8 = 0;
        else if (strcmp(ffn_q8, "1") == 0) m->ffn_q8 = 1;
        else if (strcmp(ffn_q8, "last16") == 0) m->ffn_q8 = 16;
        else if (strcmp(ffn_q8, "last32") == 0) m->ffn_q8 = 32;
        else goto error;
    }
    if (m->ffn_q8) fputs("Bonsai prefill: Q8 FFN gate/up\\n", stderr);
    CUdevice device;""")
change('"bonsai_norm_wide_128_0",', '"bonsai_norm_wide_128_0", "bonsai_pack_q8_128", "bonsai_pq2_mmq_int8",')
change("    m->q8_input = 0;\n    int32_t positions",
       "    m->q8_input = 0;\n    m->ffn_q8_input = 0;\n    m->ffn_layer = 0;\n    int32_t positions")
out.write_text(src)
