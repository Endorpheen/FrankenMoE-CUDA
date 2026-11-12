# EXP354: tiny hybrid fixtures abort on both lines, with different diagnostics

The three tiny graph fixtures built for P6-06 were the obvious candidates for a P2-17 dense/recurrent coverage run against the merged candidate.
They do not load, on either line.

## What happens

    native: /home/.../ggml/src/ggml-cuda/gated_delta_net.cu:238: fatal error
    ours:   experiments/.../EXP-2026-11-08-172-goose-cuda-gdn/source/scalar-bridge.cuh:20:
            Goose GDN validation failed: code=20
            #6 fg174s_fail () from libggml-cuda.so.0

Both abort. So the first probe's "three models, zero differences" was a **vacuous equality**: both arms reported `NOHEALTH` because both
crashed during load, and that is not evidence of anything. It is recorded here instead of being presented as a pass.

## What it does tell us

The underlying path -- gated delta net on these fixtures -- is unsupported in both builds, so the fixture cannot serve the purpose it was built
for in a graph-coverage run. That is a fixture limitation, not a migration defect.

The diagnostics do differ, and that part is real: native reports a fatal error from the CUDA source, ours raises a Goose validation code (20)
through `fg174s_fail` in the GDN scalar bridge. Both abort, so the outcome matches, but an operator reading the log sees a different message and a
different failure origin. Whether that counts as a compatibility defect depends on whether the bridge is meant to reproduce native fatal
messages as well as native outcomes -- a question worth putting to whoever owns the GDN bridge, not something to paper over.
