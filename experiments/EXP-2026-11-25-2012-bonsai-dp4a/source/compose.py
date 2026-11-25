"""Wire optional Q8 one-token projections into the diagnostic decode phase."""
from pathlib import Path
import sys

base, out = map(Path, sys.argv[1:3])
http = sys.argv[3:] == ['--http']
src = base.read_text()


def change(a, b):
    global src
    assert src.count(a) == 1, a
    src = src.replace(a, b)


change("FLASH_GROUP4, KERNEL_COUNT", "FLASH_GROUP4, MV_PACK_Q8, MV_Q8_1, MV_Q8_2, KERNEL_COUNT")
change("    const char *decode_kernel;", """    const char *decode_kernel;
    CUdeviceptr q8_input;
    int q8_mode, in_decode;""")
change("            m->packed_source = 0;\n            return launch(m, ROTATE",
       "            m->packed_source = 0;\n            m->q8_input = 0;\n            return launch(m, ROTATE")
change("                const int grouped = m->matvec_rows2 && out_width > 1024;",
       """                const int ffn = p[2] == (CUdeviceptr)m->b[14] ||
                    p[2] == (CUdeviceptr)m->b[15] || p[2] == (CUdeviceptr)m->b[18];
                const int use_q8 = m->in_decode && out_width > 1024 &&
                    (m->q8_mode == 1 || (m->q8_mode == 2 && ffn) ||
                     (m->q8_mode == 3 && out_width == VOCAB));
                if (use_q8) {
                    if (m->q8_input != p[1]) {
                        unsigned groups = in_width / 32;
                        if (launch(m, MV_PACK_Q8, (groups+3)/4, 1, 1, 128,
                            (void *[]){ &p[1], &m->packed, &groups })) return -1;
                        m->q8_input = p[1];
                    }
                    const int vocabulary = out_width == VOCAB;
                    unsigned rows_per_cta = vocabulary ? 4 : 8;
                    return launch(m, vocabulary ? MV_Q8_1 : MV_Q8_2,
                        (out_width+rows_per_cta-1)/rows_per_cta, 1, 1, 128,
                        (void *[]){ &p[0], &m->packed, &p[2], &in_width, &out_width });
                }
                const int grouped = m->matvec_rows2 && out_width > 1024;""")
change('    CUdevice device;', '''    const char *q8 = getenv("BONSAI_DECODE_Q8");
    m->q8_mode = 1;
    if (q8) {
        if (strcmp(q8, "off") == 0) m->q8_mode = 0;
        else if (strcmp(q8, "full") == 0) m->q8_mode = 1;
        else if (strcmp(q8, "ffn") == 0) m->q8_mode = 2;
        else if (strcmp(q8, "vocab") == 0) m->q8_mode = 3;
        else goto error;
    }
    if (m->q8_mode) fprintf(stderr, "Bonsai Q8 decode: %s\\n", q8 ? q8 : "full");
    CUdevice device;''')
change('"bonsai_flash_tile_group4",', '''"bonsai_flash_tile_group4",
        "bonsai_mv_pack_q8", "bonsai_pq2_mvq_q8_1", "bonsai_pq2_mvq_q8_2",''')
change("    m->packed_source = 0;\n    int32_t positions", "    m->packed_source = 0;\n    m->q8_input = 0;\n    int32_t positions")
change("    const double decoding = seconds();", "    m->in_decode = 1;\n    const double decoding = seconds();")
if http:
    end = "    if (!status) {\n        /* Attention writes new rows before reading only the active prefix. */"
    change(end, end + "\n        m->in_decode = 0;")
    src += """
int bonsai_session_step_phase(struct bonsai_session *session, const uint32_t *ids,
    unsigned count, float *logits, int decode) {
    session->runtime->in_decode = decode;
    const int status = bonsai_session_step(session, ids, count, logits);
    session->runtime->in_decode = 0;
    return status;
}
"""
out.write_text(src)
