"""Add device argmax to the isolated speculative Bonsai CLI."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, adapter, output = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "VERIFY_ATTN, KERNEL_COUNT", "VERIFY_ATTN, CHOICE_TILES, CHOICE_FINAL, KERNEL_COUNT")
text = replace(text, "    int draft_batch;", """    int draft_batch, device_choice;
    CUdeviceptr choice_mask, choice_tiles, choice_result;""")
text = replace(text, '"bonsai_verify_q4_gqa2",', '"bonsai_verify_q4_gqa2", "bonsai_choice_tiles", "bonsai_choice_final",')
text = replace(text, "static int evaluate_batch(model *m, const unsigned *tokens, unsigned count, float *logits) {",
               """static int evaluate_target(model *m, const unsigned *tokens, unsigned count,
                           float *logits, int want_logits) {""")
text = replace(text, "(logits && count > VERIFY_ROWS)", "(want_logits && count > VERIFY_ROWS)")
text = replace(text, "tokens[0], pos, logits != NULL);", "tokens[0], pos, want_logits);")
text = replace(text, "int64_t bonsai_mtp_eh(", """static int evaluate_batch(model *m, const unsigned *tokens, unsigned count, float *logits) {
    return evaluate_target(m, tokens, count, logits, logits != NULL);
}

int64_t bonsai_mtp_eh(""")
text = replace(text, '#include "greedy.inc"', '#include "choice.inc"\n#include "greedy.inc"')
(output / "backend.c").write_text(text)

greedy = adapter.read_text()
greedy = replace(greedy, "    const size_t plane = (size_t)m->context * KV_BYTES;",
                 """    const char *device_choice = getenv("BONSAI_MTP_DEVICE_CHOICE");
    m->device_choice = device_choice && strcmp(device_choice, "1") == 0;
    if (m->device_choice && choice_open(m)) return -1;
    const size_t plane = (size_t)m->context * KV_BYTES;""")
greedy = replace(greedy, "static int draft_step(model *m, unsigned token, unsigned pos, CUdeviceptr hidden, float *logits) {",
                 """static int draft_forward(model *m, unsigned token, unsigned pos, CUdeviceptr hidden,
                         float *logits, int want_logits) {""")
greedy = replace(greedy, "rc = (int)(logits ? bonsai_mtp_head", "rc = (int)(want_logits ? bonsai_mtp_head")
greedy = replace(greedy, "static int draft_catchup(", """static int draft_step(model *m, unsigned token, unsigned pos, CUdeviceptr hidden, float *logits) {
    return draft_forward(m, token, pos, hidden, logits, logits != NULL);
}

static int draft_catchup(""")
greedy, marker, _ = greedy.partition("static int greedy_mtp(")
assert marker
greedy += '\n#include "greedy-device.inc"\n'
(output / "greedy.inc").write_text(greedy)
