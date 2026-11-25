"""Add optional batched prompt-only draft KV population."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, adapter, output = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "VERIFY_CONV, KERNEL_COUNT", "VERIFY_CONV, DRAFT_CONCAT, DRAFT_MAT_BATCH, KERNEL_COUNT")
text = replace(text, "    int64_t draft_kv[2];", "    int draft_batch;\n    CUdeviceptr draft_rope_k;\n    int64_t draft_kv[2];")
text = replace(text, '"bonsai_conv_verify_prefix",', '''"bonsai_conv_verify_prefix",
        "bonsai_mtp_concat", "bonsai_mtp_q4_four",''')
text = replace(text, '#include "greedy.inc"', '#include "greedy.inc"\n#include "prefill-batch.inc"')
text = replace(text, "use_draft && draft_catchup(m, ids + at, count, 0)", "use_draft && draft_prefill(m, ids + at, count)")
text = replace(text, "    const double prefill = seconds() - start;", """    const double prefill = seconds() - start;
    const char *cache_capture = getenv("BONSAI_MTP_KV_CAPTURE");
    if (!status && use_draft && cache_capture && dump_draft_cache(m, cache_capture)) status = 1;""")
(output / "backend.c").write_text(text)

greedy = adapter.read_text()
greedy = replace(greedy, "    const size_t bytes = 2 * plane + 2 * DRAFT_HIDDEN_BYTES;",
                 """    const char *draft_batch = getenv("BONSAI_MTP_BATCH_PREFILL");
    m->draft_batch = draft_batch && strcmp(draft_batch, "1") == 0;
    const size_t bytes = 2 * plane + 2 * DRAFT_HIDDEN_BYTES +
        (m->draft_batch ? 27 * sizeof(int64_t) : 0);""")
greedy = replace(greedy, "    m->draft_chain = m->draft_carry + DRAFT_HIDDEN_BYTES;",
                 """    m->draft_chain = m->draft_carry + DRAFT_HIDDEN_BYTES;
    if (m->draft_batch) m->draft_rope_k = m->draft_chain + DRAFT_HIDDEN_BYTES;""")
(output / "greedy.inc").write_text(greedy)
