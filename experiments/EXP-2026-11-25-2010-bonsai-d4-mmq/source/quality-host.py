"""Use D4 prefill only for the already isolated Bonsai gate/up scope."""
from pathlib import Path
import sys

path, output = map(Path, sys.argv[1:])
s = path.read_text()


def change(a, b):
    global s
    assert s.count(a) == 1, a
    s = s.replace(a, b)


change("RESIDUAL_MMQ, KERNEL_COUNT", "RESIDUAL_MMQ, D4_PACK, D4_MMQ, KERNEL_COUNT")
change('        else if (strcmp(quant, "last16") == 0) m->ffn_quant_mode = 3;',
       '''        else if (strcmp(quant, "last16") == 0) m->ffn_quant_mode = 3;
        else if (strcmp(quant, "d4") == 0) m->ffn_quant_mode = 4;''')
change('        "bonsai_pack_q8_128", "bonsai_pack_q8_residual", "bonsai_pq2_mmq_int8", "bonsai_pq2_mmq_q8_residual",',
       '''        "bonsai_pack_q8_128", "bonsai_pack_q8_residual", "bonsai_pq2_mmq_int8", "bonsai_pq2_mmq_q8_residual",
        "bonsai_pack_d4", "bonsai_pq2_mmq_d4_64",''')
change("                    const int residual = m->ffn_quant_mode != 1;",
       '''                    const int d4 = m->ffn_quant_mode == 4;
                    const int residual = m->ffn_quant_mode == 2 || m->ffn_quant_mode == 3;''')
change("                    unsigned groups = count * (in_width / 128);",
       "                    unsigned groups = count * (in_width / (d4 ? 32 : 128));")
change("                            : launch(m, Q8_PACK, (groups+3)/4, 1, 1, 128,",
       "                            : launch(m, d4 ? D4_PACK : Q8_PACK, d4 ? (groups+15)/16 : (groups+3)/4, 1, 1, 128,")
change("                        : launch(m, Q8_MMQ, (out_width+63)/64, (count+63)/64, 1, 256,",
       "                        : launch(m, d4 ? D4_MMQ : Q8_MMQ, (out_width+63)/64, (count+63)/64, 1, 256,")
output.write_text(s)
