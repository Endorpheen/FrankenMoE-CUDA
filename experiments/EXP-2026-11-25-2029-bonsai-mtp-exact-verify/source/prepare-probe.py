"""Add the exact-state gate and isolate timing from the diagnostic snapshots."""
from pathlib import Path
import sys


base, output = map(Path, sys.argv[1:])
text = base.read_text()
anchor = "static int probe_target("
assert text.count(anchor) == 1
text = text.replace(anchor, """static double seconds(void);

static int measure_target(model *m, struct checkpoint *before, const unsigned *tokens,
                          unsigned rows, float *logits, const float *serial) {
    const unsigned batched[] = {0, 1, 0, 1, 1, 0};
    for (unsigned sample = 0; sample < sizeof(batched) / sizeof(*batched); ++sample) {
        if (checkpoint_io(m, before, rows, RESTORE, NULL) ||
            check(bonsai_cuda.fn_cuCtxSynchronize(), "target timing boundary")) return -1;
        m->count = 0;
        const double start = seconds();
        for (unsigned t = 0; t < VERIFY_ROWS;) {
            const unsigned count = batched[sample] ? VERIFY_ROWS : 1;
            if (evaluate_batch(m, tokens + t, count, logits + (size_t)t * VOCAB)) return -1;
            t += count;
        }
        const double elapsed = seconds() - start;
        if (memcmp(logits, serial, (size_t)VERIFY_ROWS * VOCAB * sizeof(float))) return -1;
        if (sample >= 2) {
            printf("{\\"timed_prompt\\":%u,\\"batch\\":%u,\\"target_ms\\":%.6f}\\n",
                   before->position, batched[sample], elapsed * 1000);
        }
    }
    return 0;
}

""" + anchor)
anchor = "    memcpy(batch_prefix, batched, 2 * logit_bytes);"
assert text.count(anchor) == 1
text = text.replace(anchor, """    if (exact_verify(m, VERIFY_ROWS) &&
        (different != 0 || memcmp(batched, serial, VERIFY_ROWS * logit_bytes) != 0)) {
        fprintf(stderr, "Exact target gate failed: differing state planes = %d\\n", different);
        goto done;
    }
""" + anchor)
anchor = "    status = 0;\ndone:"
assert text.count(anchor) == 1
text = text.replace(anchor, """    if (getenv("BONSAI_MTP_VERIFY_TIMING") &&
        measure_target(m, &before, tokens, snapshot_rows, batched, serial)) goto done;
""" + anchor)
output.write_text(text)
