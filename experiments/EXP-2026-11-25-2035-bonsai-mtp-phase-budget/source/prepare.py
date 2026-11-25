"""Add NVTX phase and matrix call-site ranges to the measured Bonsai adapter."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


backend, adapter, out = map(Path, sys.argv[1:])
text = backend.read_text()
text = replace(text, "#include <time.h>", """#include <time.h>
#include <nvtx3/nvToolsExt.h>

static double profile_cpu_seconds(void) {
    struct timespec time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000;
}""")
text = replace(text, "int64_t bonsai_launch(uint64_t", "static int64_t launch_op(uint64_t")
text = replace(text, "static sl_i64 slice(", """int64_t bonsai_launch(uint64_t context, int64_t op, int64_t a, int64_t b, int64_t c,
                      int64_t d, int64_t e, int64_t f, int64_t g, int64_t h) {
    if (op != MATRIX) return launch_op(context, op, a, b, c, d, e, f, g, h);
    const model *m = (model *)(uintptr_t)context;
    const char *name = c == m->b[14] ? "matrix.ffn_gate" :
                       c == m->b[15] ? "matrix.ffn_up" :
                       c == m->b[18] ? "matrix.ffn_down" :
                       c == m->b[19] ? "matrix.vocabulary" : "matrix.other";
    nvtxRangePushA(name);
    const int64_t status = launch_op(context, op, a, b, c, d, e, f, g, h);
    nvtxRangePop();
    return status;
}

static sl_i64 slice(""")
text = replace(text, "    const double start = seconds();\n    for (unsigned at",
               """    const double cpu_prefill = profile_cpu_seconds();
    nvtxRangePushA("bonsai.prefill");
    const double start = seconds();
    for (unsigned at""")
text = replace(text, """        if (evaluate_batch(m, ids + at, count, remaining == 1 ? logits : NULL)) status = 1;
        if (!status && use_draft && draft_prefill(m, ids + at, count)) status = 1;""", """
        nvtxRangePushA("pass.target_prefill");
        if (evaluate_batch(m, ids + at, count, remaining == 1 ? logits : NULL)) status = 1;
        nvtxRangePop();
        if (!status && use_draft) {
            nvtxRangePushA("pass.draft_prefill");
            if (draft_prefill(m, ids + at, count)) status = 1;
            nvtxRangePop();
        }""")
text = replace(text, "    const double prefill = seconds() - start;", """    nvtxRangePop();
    const double prefill = seconds() - start;
    fprintf(stderr, "profile_prefill_cpu_ms=%.3f\\n", 1000 * (profile_cpu_seconds() - cpu_prefill));""")
text = replace(text, "    const double decoding = seconds();", """    const double cpu_decode = profile_cpu_seconds();
    nvtxRangePushA("bonsai.decode");
    const double decoding = seconds();""")
text = replace(text, "    if (!status) fprintf(stderr,\n        \"prompt_n=", """    nvtxRangePop();
    fprintf(stderr, "profile_decode_cpu_ms=%.3f\\n", 1000 * (profile_cpu_seconds() - cpu_decode));
    if (!status) fprintf(stderr,
        "prompt_n=""")
(out / "profile-backend.c").write_text(text)

greedy = adapter.read_text()
for declare, elapsed, name in (
    ("const double draft_start", "draft_time +=", "pass.draft_decode"),
    ("const double target_start", "target_time +=", "pass.target_verify"),
    ("const double catchup_start", "catchup_time +=", "pass.draft_repair"),
):
    greedy = replace(greedy, "        " + declare, f'        nvtxRangePushA("{name}");\n        ' + declare)
    greedy = replace(greedy, "        " + elapsed, "        nvtxRangePop();\n        " + elapsed)
greedy = replace(greedy, """            if (evaluate_target(m, &anchor, 1, host_logits, 1)) return -1;""", """
            nvtxRangePushA("pass.serial_tail");
            if (evaluate_target(m, &anchor, 1, host_logits, 1)) return -1;""")
greedy = replace(greedy, "                next = choices[0];\n            }\n            continue;",
                 """                next = choices[0];
            }
            nvtxRangePop();
            continue;""")
(out / "greedy-device.inc").write_text(greedy)
