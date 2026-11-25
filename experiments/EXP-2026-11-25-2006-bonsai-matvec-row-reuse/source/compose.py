"""Wire two-row exact PQ2 matvec into the EXP-2005 model path."""
from pathlib import Path
import sys

path, out = map(Path, sys.argv[1:])
src = path.read_text()


def replace(old, new):
    global src
    if src.count(old) != 1:
        raise SystemExit(f"expected one host anchor: {old!r}")
    src = src.replace(old, new)


replace("DECODE_GQA3, KERNEL_COUNT", "DECODE_GQA3, MATRIX_ROWS2, KERNEL_COUNT")
replace("    int grouped_decode;", "    int grouped_decode, matvec_rows2;")
replace("                return launch(m, MATRIX, (out_width+3)/4, 1, 1, 128,",
        """                const int grouped = m->matvec_rows2 && out_width > 1024;
                const unsigned rows_per_cta = grouped ? 8 : 4;
                return launch(m, grouped ? MATRIX_ROWS2 : MATRIX,
                    (out_width + rows_per_cta - 1) / rows_per_cta, 1, 1, 128,""")
replace('    if (!mode || strcmp(mode, "ordered") == 0) {',
        '''    m->matvec_rows2 = !mode || strcmp(mode, "rows2") == 0;
    if (m->matvec_rows2 || strcmp(mode, "ordered") == 0) {''')
replace('    fprintf(stderr, "Decode matrix: %s\\n", m->decode_kernel);',
        '    fprintf(stderr, "Decode matrix: %s\\n", m->matvec_rows2 ? "bonsai_pq2_matvec_rows2" : m->decode_kernel);')
replace('"bonsai_decode_gqa3",', '"bonsai_decode_gqa3", "bonsai_pq2_matvec_rows2",')
out.write_text(src)
