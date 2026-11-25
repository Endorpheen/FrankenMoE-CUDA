"""Add bounded batched output and rollback tracing to the isolated Bonsai adapter."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "       GDN_STATE =", "       VERIFY_ROWS = 3, GDN_STATE =")
text = replace(text, "words[i] * sizeof(float) * (i < 19 ? m->ubatch : 1)",
               "words[i] * sizeof(float) * (i < 19 ? m->ubatch : i == 19 ? VERIFY_ROWS : 1)")
text = replace(text, "(logits && count != 1)", "(logits && count > VERIFY_ROWS)")
text = replace(text, "logits, m->b[19], VOCAB * sizeof(float)", "logits, m->b[19], (size_t)count * VOCAB * sizeof(float)")
text = replace(text, "static double seconds(void) {", '#include "target-probe.inc"\n\nstatic double seconds(void) {')
text = replace(text, "    const double decoding = seconds();", '''    if (getenv("BONSAI_MTP_TARGET_PROBE")) {
        if (!status) status = probe_target(m, logits, ignore_eos) != 0;
        goto cleanup;
    }
    const double decoding = seconds();''')
output.write_text(text)
