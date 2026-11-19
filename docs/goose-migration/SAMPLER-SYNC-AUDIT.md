# Four context synchronisations per generated token, in the sampler path

Found 2026-11-20 while chasing a 3-5% decode gap. This is not the JSON work and not the compiler work; it
is the sampler path in `vendor/flint/common/sampling.cpp`.

## What the accessors do

`vendor/flint/src/llama-context.cpp:4382-4398`:

```cpp
float * llama_get_sampled_probs_ith(llama_context * ctx, int32_t i) {
    ctx->synchronize();
    return ctx->get_sampled_probs_ith(i);
}
```

and the same for `llama_get_sampled_logits_ith` and `llama_get_sampled_candidates_ith`. Six such accessors
in that file synchronise; the context has 24 `synchronize()` calls in total.

## Where they are called per token

| site | calls | why it is on the hot path |
|---|---|---|
| `common_sampler_sample`, the crash guard added in EXP-1438 | 1 | **fixed today** - the device question moved inside the `id < 0 \|\| id >= n_vocab` branch |
| `common_sampler::set_logits` (`sampling.cpp:137-139`) | 3 | called once per sampled token, to hand the chain its candidate arrays |

So a generated token cost **four** pipeline synchronisations before today's fix, and it costs **three** now.

## What is not yet known

Whether any of them is necessary. A synchronise makes the pointers valid when the data was produced on the
device, so removing one may return stale data - a correctness risk, not a speed trade. The fix applied
today is safe because the value was only needed to print a diagnostic that fires almost never.

The `set_logits` ones are a different case: they are load-bearing if the candidate arrays live on the
device. The cheap, safe experiment is to synchronise **once** per token (`llama_synchronize(ctx)`) and then
use the non-synchronising internal getters, which turns three waits into one - but it needs a build and a
measurement, and it is recorded here rather than assumed.

## How this was found, because the method matters

Not by reading code for its own sake: the JSON set measured 5% slower than the baseline, both libraries were
compared, neither differed in its dynamic section, and the difference had to be inside the code. Reading the
sampler path for anything that waits on the device produced this in one pass. The 5% between the two builds
is still unexplained, but the four synchronisations were sitting there regardless.

## A nuance that keeps the claim honest

Four *calls* per token is not four *costs*. `ctx->synchronize()` drains pending work; the first call after
the graph is computed waits for it, and the next two find nothing to wait for and return almost immediately.
So the plausible cost is **one pipeline drain per token**, not four, and the gain from removing the extra
calls is likely small.

Also verified, because I had asserted it: `set_logits` is called from inside `common_sampler_sample`
(`sampling.cpp:671`) and again on the resampling path (`:766`), so it really is once per sampled token -
not once per request as one might hope.

The fix applied today removed a call that was pure overhead (its value was only printed in a rare branch).
The remaining three are load-bearing and their removal needs the experiment described above, measured
against the baseline rather than assumed to help.
