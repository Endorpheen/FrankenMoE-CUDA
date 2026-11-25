"""Preserve existing Bonsai prefill reuse and optimize only small decay steps."""
from pathlib import Path
import sys

base, out = map(Path, sys.argv[1:])
src = base.read_text()


def replace(a,b):
    global src
    assert src.count(a)==1,a
    src=src.replace(a,b)


replace("FFN_Q8_MMQ, KERNEL_COUNT","FFN_Q8_MMQ, DECAY_STAGE256, DECAY_WARPPACK, KERNEL_COUNT")
replace("    unsigned ffn_layer;","    unsigned ffn_layer;\n    int decay_small;")
replace("        case DECAY:\n            if (m->decay_reuse",
        """        case DECAY:
            if (m->decay_small && count == 1) {
                return launch(m, DECAY_STAGE256, 48, 1, 1, 256,
                    (void *[]){ &p[0], &p[1], &p[2], &p[3], &p[4], &p[5], &p[6] });
            }
            if (m->decay_small && count < 32) {
                unsigned tokens=count;
                return launch(m, DECAY_WARPPACK, 12 * count, 1, 1, 128,
                    (void *[]){ &p[0], &p[1], &p[2], &p[3], &p[4], &p[5], &p[6], &tokens });
            }
            if (m->decay_reuse""")
replace('    const char *decay=getenv("BONSAI_DECAY_REUSE");',
        '''    const char *small=getenv("BONSAI_DECAY_SMALL");
    m->decay_small=!small || strcmp(small,"1")==0;
    if (m->decay_small) fputs("Bonsai decay: staged single-token products\\n",stderr);
    const char *decay=getenv("BONSAI_DECAY_REUSE");''')
replace('"bonsai_pq2_mmq_int8",','"bonsai_pq2_mmq_int8", "bonsai_decay_stage_256_0", "bonsai_decay_warp_pack",')
out.write_text(src)
