"""Dispatch only the winning causal-tail kernel in the exact verifier."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


backend, operations, output = map(Path, sys.argv[1:])
text = backend.read_text()
text = replace(text, "FLASH_Q_PACK, KERNEL_COUNT", "FLASH_Q_PACK, VERIFY_ATTN_TAIL, KERNEL_COUNT")
text = replace(text, "verify_attention, verify_merge;", "verify_attention, verify_merge, verify_attn_tail;")
text = replace(text, "    m->q8_mode = 1;", """    const char *attn_tail = getenv("BONSAI_MTP_VERIFY_ATTN_TAIL");
    m->verify_attn_tail = attn_tail && strcmp(attn_tail, "1") == 0;
    m->q8_mode = 1;""")
text = replace(text, '"bonsai_pack_query_panels",', '"bonsai_pack_query_panels", "bonsai_verify_q4_tail",')
(output / "backend.c").write_text(text)
ops = replace(operations.read_text(), "launch(m, VERIFY_ATTN, 12, max_tiles, 1, 32,",
              "launch(m, m->verify_attn_tail ? VERIFY_ATTN_TAIL : VERIFY_ATTN, 12, max_tiles, 1, 32,")
(output / "verify-ops.inc").write_text(ops)
