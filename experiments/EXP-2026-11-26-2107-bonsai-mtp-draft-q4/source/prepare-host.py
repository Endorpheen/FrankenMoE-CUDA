"""Use the eight-token Q4 projection for complete draft-prefill blocks."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
host = (base / "backend.c").read_text()
draft = (base / "draft.inc").read_text()
host = replace(host, "VERIFY_MVQ_FIVE_QUAD, KERNEL_COUNT",
               "VERIFY_MVQ_FIVE_QUAD, DRAFT_MAT_BATCH_WIDE, KERNEL_COUNT")
host = replace(host, "if (i > CHOICE_FINAL) {", "if (i == DRAFT_MAT_BATCH_WIDE) continue;\n        if (i > CHOICE_FINAL) {")
host = replace(host, "        if (!m->choice_mask || draft_open(m)) goto error;",
               """        if (!m->choice_mask || draft_open(m)) goto error;
        const char *q4_path = getenv("BONSAI_HTTP_DRAFT_Q4_IMAGE");
        if (q4_path && *q4_path) {
            CUmodule q4_module;
            if (check(bonsai_cuda.fn_cuModuleLoad(&q4_module, q4_path), "draft Q4 image") ||
                check(bonsai_cuda.fn_cuModuleGetFunction(&m->kernel[DRAFT_MAT_BATCH_WIDE],
                    q4_module, "bonsai_mtp_q4_8"), "draft Q4 eight-token function")) goto error;
        }""")
draft = replace(draft, "    return launch(m, DRAFT_MAT_BATCH, nrows/4, (tokens+3)/4, 1, 128,",
                """    const int wide = tokens == 128 && m->kernel[DRAFT_MAT_BATCH_WIDE];
    const unsigned group = wide ? 8 : 4;
    return launch(m, wide ? DRAFT_MAT_BATCH_WIDE : DRAFT_MAT_BATCH,
                  nrows / 4, (tokens + group - 1) / group, 1, 128,""")
(out / "backend.c").write_text(host)
(out / "draft.inc").write_text(draft)
