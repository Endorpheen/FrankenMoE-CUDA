"""Try one early FFN projection in Q8 while retaining late-layer defaults."""
from pathlib import Path
import sys

source,out=map(Path,sys.argv[1:])
s=source.read_text()


def change(a,b):
    global s
    assert s.count(a)==1,a
    s=s.replace(a,b)


change("    unsigned ffn_layer;", "    unsigned ffn_layer;\n    int ffn_q8_early;")
change("""            if (m->ffn_q8 && (ffn_gate || ffn_up) && scale_bytes <= m->partial_capacity &&
                (m->ffn_q8 == 1 || m->ffn_layer > (unsigned)(LAYERS - m->ffn_q8))) {""",
        """            const int late_q8 = m->ffn_q8 &&
                (m->ffn_q8 == 1 || m->ffn_layer > (unsigned)(LAYERS - m->ffn_q8));
            const int early_q8 = (m->ffn_q8_early == 1 && ffn_gate) ||
                                 (m->ffn_q8_early == 2 && ffn_up);
            if ((late_q8 || early_q8) && (ffn_gate || ffn_up) && scale_bytes <= m->partial_capacity) {""")
change('    if (m->ffn_q8) fputs("Bonsai prefill: Q8 FFN gate/up\\n", stderr);',
       '''    if (m->ffn_q8) fputs("Bonsai prefill: Q8 FFN gate/up\\n", stderr);
    const char *early = getenv("BONSAI_FFN_Q8_EARLY");
    if (early && strcmp(early, "0") != 0) {
        if (strcmp(early, "gate") == 0) m->ffn_q8_early = 1;
        else if (strcmp(early, "up") == 0) m->ffn_q8_early = 2;
        else goto error;
        fprintf(stderr, "Diagnostic Bonsai early FFN Q8: %s\\n", early);
    }''')
out.write_text(s)
