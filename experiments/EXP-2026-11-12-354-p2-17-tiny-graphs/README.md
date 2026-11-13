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

## A real crash in the merged build, found by these fixtures

Once `-ngl 0` let both lines load `tiny-hybrid`, the merged candidate died where native answered. The response was an empty completion natively
and a dropped connection from us, and the log said:

    FG272S validation code 10
    #1 fg272s_pick_similar_impl
    #2 server_context_impl::get_available_slot
    #3 server_context_impl::process_single_task

Reading the generated module shows why: `pick_similar` validates `count >= 0 && n_tokens > 0` and fails with code 10 otherwise. The merged
call site passed `task.tokens.size()`, which was zero for this request, so the bridge aborted where the native code simply had nothing to
compare.

With `GGML_GOOSE_SLOT_SELECT=0` the same request returns the native empty completion and no validation failure, which pinned the blame on the
merged slot-selection port rather than on the model or the fixture.

**Fix:** the call site now requires a non-empty prompt before handing the decision to Goose, because a picker with no tokens to compare is not
a decision at all. After the relink, `tiny-hybrid` returns the same empty completion as native.

This matters beyond the fixture: the slot-selection port was not in the deployed candidate before this merge, so this crash would have appeared
first in an Apex run -- and it would have looked like a hardware or model problem rather than a missing precondition.

`tiny-rr` and `tiny-mutant` still report no health on both lines, so for them the equality remains vacuous and is not counted.
