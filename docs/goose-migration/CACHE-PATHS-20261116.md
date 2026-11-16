# The two expert-cache paths, and a correction

Written to close the open question from CACHE-OBSERVABILITY-20261116 ("copied_from_cache freezes after
prefill") and to correct a wrong conclusion I drew while chasing it.

## Correction first

I wrote that `copy()` is never called, because a `LLAMA_LOG_INFO` probe inside it never printed. **That was
wrong.** A probe writing `plan()` calls to a file recorded **21k calls** for one 512-token request, and
`plan()` is called only from `copy()`. The conclusion should have been "LLAMA_LOG_INFO from this file does
not reach the log" - which I established separately - not "the function does not run".

The lesson is the same one that has recurred all session: a silent probe is only evidence about the probe.

## The two paths, which explains the frozen counter

**Decode** (small batch, `get_slot_map` returns the layer's map):

```cpp
// llama-graph.cpp:1724
ggml_mul_mat_id(ctx0, moe_cache->get_experts(w), cur, slots);
```

The cached expert bank is read **in place**, selected by slot. No copy and no upload on a hit. Misses are
still filled by the copy callback (`copy()` -> `slru.plan` -> upload), which is why uploads keep growing
during decode.

**Prefill** (large batch, `get_slot_map` returns `nullptr` because `n_tokens > max_batch`):

```cpp
// llama-context.cpp:2975
const int64_t n = lctx->moe_cache->copy_experts(backend, src, dst, e, last);
...
ggml_backend_tensor_set_async(backend, dst, ...);   // the experts not in the cache
```

Here the cache is used as a *source*: consecutive cached experts are copied device-to-device into the
stacked expert tensor, and the rest are uploaded from host memory. That is the path that increments
`copied_from_cache`.

**So the frozen counter is correct behaviour, not a defect.** It counts prefill's device-to-device copies;
decode never takes that path because it reads the bank directly.

## What the numbers now say together

| phase | mechanism | observed |
|---|---|---|
| prefill | `copy_experts` + upload | `copied_from_cache=16596`, 9970 MiB device-to-device |
| decode | `mul_mat_id` in place | hits served with no copy; misses uploaded |
| both | `copy()` -> `slru.plan` | 81.4% hit rate, 2.8 MiB uploaded per plan |

## Where an optimisation could actually go

The upload volume is the visible cost: **2.8 MiB per plan at an 81.4% hit rate**. Two directions, both
measurable with the counters now in place:

1. **Raise the hit rate.** The SLRU is now the Goose policy (EXP-756), so a change there is a change to one
   Goose module and its gate - not to C++.
2. **Reduce bytes per miss.** 2.8 MiB/plan over ~18.6% misses suggests large expert rows per miss. Whether
   that is the Q4_K_M row size or partial-expert granularity needs one measurement, not a guess.

Neither is attempted here. The point of this card is that the two paths are now documented and the counters
exist to judge them.
