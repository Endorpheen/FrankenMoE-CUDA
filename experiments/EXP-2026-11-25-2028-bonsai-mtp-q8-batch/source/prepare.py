"""Add optional three-token Q8 projection to the diagnostic target adapter."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "MTP_MAT, KERNEL_COUNT", "MTP_MAT, VERIFY_Q8_1, VERIFY_Q8_2, KERNEL_COUNT")
text = replace(text, '    int q8_mode, in_decode', '    int verify_q8;\n    int q8_mode, in_decode')
text = replace(text, '    m->q8_mode = 1;', '''    const char *verify_q8 = getenv("BONSAI_MTP_VERIFY_Q8");
    m->verify_q8 = verify_q8 && strcmp(verify_q8, "1") == 0;
    m->q8_mode = 1;''')
text = replace(text, '"bonsai_mtp_mat_q4",', '''"bonsai_mtp_mat_q4",
        "bonsai_pq2_mvq_q8_three_1", "bonsai_pq2_mvq_q8_three_2",''')
text = replace(text, '            unsigned pairs = count * in_width / 2, tokens = count;', '''
            if (count == VERIFY_ROWS && m->verify_q8 && m->in_decode && m->q8_mode == 1 &&
                out_width > 1024) {
                if (m->q8_input != p[1]) {
                    unsigned groups = count * (in_width / 32);
                    if (launch(m, MV_PACK_Q8, (groups + 3) / 4, 1, 1, 128,
                               (void *[]){ &p[1], &m->packed, &groups })) return -1;
                    m->q8_input = p[1];
                }
                const int vocabulary = out_width == VOCAB;
                const unsigned rows_per_cta = vocabulary ? 4 : 8;
                return launch(m, vocabulary ? VERIFY_Q8_1 : VERIFY_Q8_2,
                    (out_width + rows_per_cta - 1) / rows_per_cta, 1, 1, 128,
                    (void *[]){ &p[0], &m->packed, &p[2], &in_width, &out_width });
            }
            unsigned pairs = count * in_width / 2, tokens = count;''')
output.write_text(text)
