"""Connect the resident drafter and exact transactional verifier in the CLI."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "    X(cuMemcpyDtoH)", "    X(cuMemcpyDtoDAsync) \\\n    X(cuMemcpyDtoH)")
text = replace(text, "    int device_commit;", """    int device_commit;
    int64_t draft_kv[2];
    CUdeviceptr draft_carry, draft_chain;""")
text = replace(text, '#include "commit-probe.inc"', '#include "commit-probe.inc"\n#include "greedy.inc"')
text = replace(text, "    const double start = seconds();\n    for (unsigned at", """    const char *draft = getenv("BONSAI_MTP_GREEDY");
    const int use_draft = draft && strcmp(draft, "1") == 0;
    if (!status && use_draft && draft_open(m)) status = 1;
    const double start = seconds();
    for (unsigned at""")
text = replace(text, "        at += count;", """        if (!status && use_draft && draft_catchup(m, ids + at, count, 0)) status = 1;
        at += count;""")
text = replace(text, "    const double prefill = seconds() - start;", """    if (!status && use_draft && check(bonsai_cuda.fn_cuCtxSynchronize(), "draft prefill complete")) status = 1;
    const double prefill = seconds() - start;""")
text = replace(text, "    for (unsigned i = 0; i < generate && !status; ++i) {", """    if (!status && use_draft) {
        status = greedy_mtp(m, logits, generate, ignore_eos, &generated) != 0;
    }
    for (unsigned i = 0; !use_draft && i < generate && !status; ++i) {""")
output.write_text(text)
