"""Wire Q8-64 into the Bonsai gate/up projections for the quality fixture."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FLASH_FUSED_ORDERED, KERNEL_COUNT",
               "FLASH_FUSED_ORDERED, Q8_64_PACK, Q8_64_MMQ, KERNEL_COUNT")
host = replace(host, "q8_mode, in_decode, norm_wide, ffn_q8;",
               "q8_mode, in_decode, norm_wide, ffn_q8, q8_64;")
host = replace(host, '    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");',
               '''    const char *q8_64 = getenv("BONSAI_FFN_Q8_64");
    m->q8_64 = q8_64 && strcmp(q8_64, "1") == 0;
    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");''')
host = replace(host, '"bonsai_flash_kv_fused_ordered",',
               '"bonsai_flash_kv_fused_ordered", "bonsai_pack_q8_64", "bonsai_pq2_mmq_q8_64_n128",')
host = replace(host, "            if (m->ffn_q8 && (ffn_gate || ffn_up)",
               '''            if (m->q8_64 && (ffn_gate || ffn_up) && 2*scale_bytes <= m->partial_capacity) {
                unsigned groups = count * (in_width / 64);
                if (m->ffn_q8_input != p[1]) {
                    if (launch(m, Q8_64_PACK, (groups + 7) / 8, 1, 1, 128,
                        (void *[]){ &p[1], &m->packed, &m->partial, &groups })) return -1;
                    m->ffn_q8_input = p[1];
                    m->packed_source = 0;
                }
                return launch(m, Q8_64_MMQ, (out_width+63)/64, (count+127)/128, 1, 256,
                    (void *[]){ &p[0], &m->packed, &m->partial, &p[2], &in_width, &out_width, &tokens });
            }
            if (m->ffn_q8 && (ffn_gate || ffn_up)''')
out.write_text(host)
