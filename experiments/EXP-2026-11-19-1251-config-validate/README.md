# EXP-2026-11-19-1251: configuration validation against the real native validator

`bmoe::validate` (runtime/core/src/config.cpp, 265 lines) is pure: it reads a `RunConfig` and returns ok or a
message. The port answers with an **ordered rule index** instead of a message, because messages are byte slices
and the comparison that matters is *which rule fired, in the native order*.

```
config validate: cases=4000 fails=0 uncovered=0
```

4000 random configurations against `bmoe::validate` linked from `libbmoe_core.a`. The harness maps the native
message back to its rule index, so the two sides are compared on the decision, not on wording. `uncovered=0`
means every configuration the native code rejected was rejected by a rule this port implements - nothing was
silently skipped.

## The 17 rules covered, in native order

| # | rule |
|---|---|
| 1 | model_path required |
| 2-4 | n_predict, n_threads, n_ctx positive |
| 5-6 | n_ubatch non-negative and not above n_ctx |
| 7 | n_expert_used non-negative |
| 8-10 | workload capture/replay: exclusive, needs moe, excludes speculative |
| 11-13 | vram_cache_mb, expert_hot_slots non-negative; VRAM cache needs n_gpu_layers |
| 14-15 | sampling.top_k non-negative, sampling.top_p in (0,1] |
| 16 | speculative requires greedy decoding |
| 17 | moe.overlap requires moe.enabled |

## What is not covered, and it is named

- the **messages**: not ported, deliberately - they are text, and text comparison would test my transcription of
  English rather than the decision;
- the **spec-specific** rules (draft_max range, draft_p_min range, the MTP/ngram p_min exclusivity): the harness
  keeps speculation off, so those branches are never entered. They are the next slice.

## Why this shape is better than a symmetric gate

The native function is compiled and called; there is no transcription anywhere. The three earlier verification
errors in this session were all transcriptions, and this shape removes that failure mode from the comparison
entirely. It also runs in under a second with no model and no GPU.

## Reproduce

```
bash experiments/EXP-2026-11-19-1251-config-validate/build.sh
```
