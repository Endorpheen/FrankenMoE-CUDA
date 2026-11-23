# P6-10: what removing the replaced sampler C++ involves

**Historical reference.** This proposal describes the old native sampler. See [the integration map](../README.md) for the standalone path.

Prepared, **not applied**. Rule 15 forbids deleting C++ before proven replacement and Igor's
acceptance, and rule 10 says an accepted feature is enabled by default with the env flag existing
only to disable it. Both constrain the removal, so this is the inventory and the exact steps.

## Evidence already on record

Nine sampler stages are ported, gated and live: typical-p 1800/1800, top-p 900/900 and 45/45,
XTC 720/720, DRY 200/200, adaptive-p 33211 transformed logits, mirostat 500/500, the RNG stream
1280/1280, and the chain addresses its entry points by exported name. Performance on the 32K pair:
prefill -0.72%, decode -0.88%, VRAM identical, RSS +0.02%.

That covers the *decisions*. It does not cover the removal, because the native body is still the
path taken when a stage flag is set to 0 - which rule 10 requires to exist.

## The bodies

`vendor/flint/src/llama-sampler.cpp` carries one hook block per stage, each guarded by
`fg_sampler_*_call != nullptr && <stage>_enabled()`:

| stage | hook site | native body it shadows |
|---|---|---|
| dist_words | line ~375 | word-sequence sampling |
| dist_pick | line ~1346 | pick by probability |
| temp_ext | line ~2395 | temperature extension |
| mirostat | line ~2790 | mirostat k and update |
| adaptive_p | line ~4196 | adaptive-p transform and EMA |
| typical, top_p, xtc, penalties, dry | earlier sites | their apply functions |

## Steps, in order

1. Turn each stage's opt-out into a no-op and delete the native body it shadows, one stage at a
   time, keeping the differential gate green after each (the gates are differential, so a deleted
   body that is still needed shows up immediately as a link error, not as a silent change).
2. Remove the shim's weak symbols and the `nullptr` guards: with no native body, the chain must be
   present or the build fails, which is the correct failure.
3. Delete `fg_goose_chain_enabled()` and the per-stage flags, leaving no switch at all.
4. Re-run the full differential set and the 32K paired measurement on the composed build, then
   rebuild the cumulative runtime and save the checkpoint.

## Why it is not started here

Step 1 deletes working code that is currently reachable through a documented opt-out. That is a
destructive action with a blast radius beyond the working tree, and the rule is explicit that it
follows acceptance rather than preceding it. Everything needed to start is in this file, so the
step can begin the moment it is approved.
