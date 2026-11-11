# EXP355: Split-equal tail policy wired into the candidate (route probe)

The journal-to-actions bridge for the EXP348 block, its call site in `split_equal`, and the run that
shows whether Apex enters it.

## Verdict

**WIRED, NOT ENTERED** — the bridge is exact, the substitution is sound, and the block is skipped on
the Apex profile because `n_keep_tail` is zero there.

## What was built

- `source/bridge.c` — calls the EXP348 module and turns its journal into three action lists
  (`extend`, `truncate`, `release`), applies the cursors and the `used` flags, and derives `n_used`
  from the actions. The first call prints a witness, so a SIGINT shutdown cannot hide it.
- `integration/llama-batch.cpp` — `split_equal` calls the bridge inside the `n_keep_tail > 0` branch
  and jumps past the native block on success; `GGML_GOOSE_BATCH_SPLIT_TAIL=0` restores it.
- `build_stage.py` — stage with **three** call sites (state-write ranges, split-simple, split-equal
  tail): 4 840 352 bytes, all 5 249 previously exported symbols kept.

## Bridge gate

| check | result |
|---|---|
| 4 000 draws, actions + cursors + used + n_used | 67 619 checks / 0 failures |
| symbol set of the integrated object | 123 = 123 |

## Apex run on the stage (32K/128)

| check | result |
|---|---|
| status | RESPONSE_RECEIVED, 128 tokens |
| content hash | `e68df05b…` — equals the control |
| tokens / probabilities | equal, 128 / 128 |
| acceptance | prefill 2514.6 tok/s, decode 25.30 tok/s |
| `FG348S` witness | **0 calls** |

## What this probe taught

The block never runs because Apex has no rollback sequences, so `n_keep_tail` is zero and the native
code — as well as the bridge — is never entered. Together with EXP354 (`split_simple` also zero) the
picture is now explicit: the batch splitter's *recurrent* paths are dormant for this profile, and the
part Apex does run is `split_equal`'s set selection and `ubatch_add`.

This probe also caught a real defect in EXP348 (deferred release and concatenation used the whole
per-set lists instead of the taken prefixes); it was invisible to that experiment's own gate because
the transcription repeated the same model. Fixed in `eae4a56`, and the counts above come from the
corrected module.

## Next live route

`ubatch_add` (EXP349): called for every ubatch, so it is the first block in this area that Apex
certainly enters.
