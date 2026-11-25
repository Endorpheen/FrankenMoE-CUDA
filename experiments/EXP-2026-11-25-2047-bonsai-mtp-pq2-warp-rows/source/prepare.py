"""Select the component-proven warp layouts without changing arithmetic."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "VERIFY_CONV_FIVE, KERNEL_COUNT",
    "VERIFY_CONV_FIVE, VERIFY_MVQ_FOUR_PAIR, VERIFY_MVQ_FOUR_QUAD, VERIFY_MVQ_FIVE_QUAD, KERNEL_COUNT")
text = replace(text, "    unsigned verify_rows;", "    unsigned verify_rows;\n    int verify_mvq_shape;")
text = replace(text, "    m->q8_mode = 1;", """    const char *mvq_shape = getenv("BONSAI_MTP_MVQ_SHAPE");
    m->verify_mvq_shape = mvq_shape && strcmp(mvq_shape, "1") == 0;
    m->q8_mode = 1;""")
text = replace(text, '"bonsai_verify_five_32", "bonsai_conv_verify_prefix_five",', '''
        "bonsai_verify_five_32", "bonsai_conv_verify_prefix_five",
        "bonsai_mvq_4_permutation_2", "bonsai_mvq_4_permutation_4", "bonsai_mvq_5_spread_4",''')
text = replace(text, """                const unsigned rows_per_cta = vocabulary ? 4 : 8;
                enum kernel projection = vocabulary ? VERIFY_Q8_1 : VERIFY_Q8_2;
                if (count == 4) projection = vocabulary ? VERIFY_Q8_FOUR_1 : VERIFY_Q8_FOUR_2;
                if (count == 5) projection = vocabulary ? VERIFY_Q8_FIVE_1 : VERIFY_Q8_FIVE_2;
                return launch""", """                unsigned rows_per_cta = vocabulary ? 4 : 8;
                enum kernel projection = vocabulary ? VERIFY_Q8_1 : VERIFY_Q8_2;
                if (count == 4) {
                    projection = vocabulary ? VERIFY_Q8_FOUR_1 : VERIFY_Q8_FOUR_2;
                    if (m->verify_mvq_shape) {
                        projection = vocabulary ? VERIFY_MVQ_FOUR_PAIR : VERIFY_MVQ_FOUR_QUAD;
                        rows_per_cta = vocabulary ? 8 : 16;
                    }
                }
                if (count == 5) {
                    projection = vocabulary ? VERIFY_Q8_FIVE_1 : VERIFY_Q8_FIVE_2;
                    if (m->verify_mvq_shape) {
                        projection = VERIFY_MVQ_FIVE_QUAD;
                        rows_per_cta = 16;
                    }
                }
                return launch""")
output.write_text(text)
