"""Allow the draft vocabulary to reuse the existing Bonsai Q8 projection."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


backend, greedy, head, output = map(Path, sys.argv[1:])
text = backend.read_text()
text = replace(text, "int draft_batch, device_choice;", "int draft_batch, device_choice, draft_q8_head;")
text = replace(text, "    m->q8_mode = 1;", """    const char *draft_q8_head = getenv("BONSAI_MTP_DRAFT_Q8_HEAD");
    m->draft_q8_head = draft_q8_head && strcmp(draft_q8_head, "1") == 0;
    m->q8_mode = 1;""")
(output / "backend.c").write_text(text)
text = replace(greedy.read_text(), "    m->in_decode = 0;",
               """    // The draft's Q4 projections bypass this flag; only its PQ2 vocabulary uses it.
    m->in_decode = want_logits && m->draft_q8_head;""")
(output / "greedy.inc").write_text(text)
text = replace(head.read_text(), "    file = fopen(path, \"wb\");",
               """    m->in_decode = m->draft_q8_head;
    file = fopen(path, "wb");""")
(output / "head-probe.inc").write_text(text)
