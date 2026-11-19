# EXP-2026-11-19-1251: configuration validation against the real native validator

`bmoe::validate` (runtime/core/src/config.cpp) is pure: it reads a `RunConfig` and returns ok or a message. The
port answers with an **ordered rule index** instead of a message, because messages are slices of text and the
comparison that matters is *which rule fired, in the native order*.

```
config validate: cases=4000 fails=0 uncovered=0
```

4000 random configurations against `bmoe::validate` linked from `libbmoe_core.a`. The harness maps the native
message back to its rule index, so the sides are compared on the decision. `uncovered=0` means every rejection
the native code produced came from a rule this port implements.

## Rules covered (native order)

| # | rule |
|---|---|
| 1-7 | model_path; n_predict, n_threads, n_ctx positive; n_ubatch non-negative and ≤ n_ctx; n_expert_used |
| 8-10 | workload capture/replay: exclusive, needs moe, excludes speculative |
| 11-13 | vram_cache_mb, expert_hot_slots; VRAM cache needs n_gpu_layers |
| 14-16 | sampling.top_k, sampling.top_p in (0,1]; speculative requires greedy |
| 17-22 | draft_max range; MTP p_min range; ngram p_min must be zero; ngram match ranges; n_ubatch ≥ draft_max+1 |
| 23-26 | overlap, predict_log, predict_prefetch, route_ahead all require moe.enabled |
| 27-28 | row_stream requires moe; row_stream_mb non-negative |
| 29-35 | nested MoE: io_threads range, cache_mb non-negative, the pathological band, prefetch_layers range, cache_auto exclusivity, cache floor/ceil |
| 36-39 | prefetch_layers and predict_prefetch need the cache; io_two_wave needs overlap and the cache |
| 40-44 | predict_spec_max range; prefetch mutual exclusion; route_ahead range, exclusions and its conflict with speculation |

## Three defects in my *harness*, found by running it

1. **`rfind(s, 0)` tests a prefix, not a substring.** The verify-batch message does not start with the text I
   looked for, so the mapping returned the wrong rule and produced 8 phantom mismatches. `find()` fixed it.
2. **The mapping order mattered.** Two messages both begin with `n_ubatch=`, so the narrower test has to run
   before the prefix test.
3. **A rule I put in the wrong block.** `route_ahead must be in [0, 8]` lives in the nested MoE block, not in
   the top-level sequence; including it produced a mismatch where the native code returned ok.

None of the three was in the port. That is the fifth verification-logic error of this session and the reason
the differential is worth more than a symmetric gate: the port was right and the test was wrong, and running it
against real code says so immediately.

## Slice boundary is now closed

The nested MoE block is included: 44 rules in total, all exercised by randomisation, `uncovered=0`. Nothing is
held valid to hide a rule.

## The prefix-versus-substring defect bit twice

The `rfind(s, 0)` mistake appeared a second time in a mapping line added later ("is in the pathological band").
The fix was to replace **every** `rfind(...) != npos` in the file at once rather than the one that failed, because
the failing one was new and the old pattern was still in the habit - not in the code.

## Reproduce

```
bash experiments/EXP-2026-11-19-1251-config-validate/build.sh
```
