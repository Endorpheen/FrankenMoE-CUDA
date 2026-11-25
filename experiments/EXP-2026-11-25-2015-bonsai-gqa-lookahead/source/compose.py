"""Use look-ahead only for the existing one-token Bonsai GQA specialization."""
from pathlib import Path
import sys

base,out=map(Path,sys.argv[1:])
s=base.read_text()


def change(a,b):
    global s
    assert s.count(a)==1,a
    s=s.replace(a,b)


change("FFN_Q8_MMQ, KERNEL_COUNT","FFN_Q8_MMQ, GQA_LOOKAHEAD_SHORT, GQA_LOOKAHEAD_LONG, KERNEL_COUNT")
change("    int grouped_decode, matvec_rows2, flash_grouped;",
       "    int grouped_decode, matvec_rows2, flash_grouped, gqa_lookahead;")
change("""                unsigned heads = keys >= 32768 ? 3 : 2;
                if (launch(m, heads == 3 ? DECODE_GQA3 : DECODE_GQA2, 24 / heads, tiles, 1, 32,""",
       """                const int lookahead = m->gqa_lookahead && keys >= 256;
                const unsigned heads = keys >= 32768 || lookahead ? 3 : 2;
                const enum kernel decode = lookahead ?
                    (keys >= 32768 ? GQA_LOOKAHEAD_LONG : GQA_LOOKAHEAD_SHORT) :
                    (heads == 3 ? DECODE_GQA3 : DECODE_GQA2);
                if (launch(m, decode, 24 / heads, tiles, 1, 32,""")
change('    const char *grouped = getenv("BONSAI_DECODE_GQA");', '''    const char *lookahead = getenv("BONSAI_GQA_LOOKAHEAD");
    m->gqa_lookahead = lookahead && strcmp(lookahead, "1") == 0;
    const char *grouped = getenv("BONSAI_DECODE_GQA");''')
change('"bonsai_pq2_mmq_int8",','"bonsai_pq2_mmq_int8", "bonsai_gqa_h3_k4_v0", "bonsai_gqa_h3_k2_v1",')
out.write_text(s)
