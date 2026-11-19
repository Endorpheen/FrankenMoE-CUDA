# EXP-2026-11-19-1384: the MTP head does not share the target's memory - the code says so

## The finding

`common_speculative_init_result` sets, for the MTP case:

```cpp
cparams.n_ctx    = llama_n_ctx(ctx_tgt);   // 262144
cparams.ctx_other = ctx_tgt;               // the target context
```

and `llama_context`'s constructor **discards** that field immediately:

```cpp
cparams.ctx_other = nullptr;
if (model.arch == LLM_ARCH_GEMMA4_ASSISTANT) { ... cparams.ctx_other = params.ctx_other; }
if (model.arch == LLM_ARCH_EAGLE3 || model.arch == LLM_ARCH_DFLASH) { ... }
```

Apex is `qwen35moe`. None of the three branches applies, so the MTP head builds a **private** context: its own
262 144-slot KV cache, its own compute buffers, its own output reserve - a second full context beside the first.

## What that costs, from the model's own metadata

The loader prints the parameters this calculation needs: 41 blocks, 2 KV heads, key and value length 256, KV cached as
`q4_0` (about 0.5625 bytes per element with block scaling).

```
per token per layer : 2 heads x 256 x 2 (K and V) x 0.5625 B  ~=  576 B
per token           : 576 B x 41 layers                     ~= 23.6 KiB
at 262144 tokens    : 23.6 KiB x 262144                     ~= 6.0 GiB
```

That is an arithmetic statement from the model's metadata and the accepted KV type, not a measurement, and it is labelled
as such. It explains the two walls already measured: with ~6 GiB taken by a second KV, the target's 1.31 GiB prefill
buffer no longer fits, and the `cudaMalloc` failures appear in whichever allocation runs next.

## Why this is the user's point, in code

The question "must the head have a second full context?" has an answer in this tree: **it does not have to, but today it
does**, because the sharing path (`ctx_other`) is only honoured for three architectures and qwen35moe is not among them.
That is an implementation limit, not a hardware one.

## What a fix would look like, and what it must not do

1. honour `ctx_other` for a same-model MTP head, so the head reads the target's KV instead of duplicating it - the field
   already exists and is already passed;
2. if full sharing is wrong for the head (it may need its own positions), give it its own KV sized to the **draft
   window** rather than to `n_ctx`, which is the same idea at a smaller scope;
3. chunk the draft feed at the eight `batch.get()` sites so a small `n_batch` becomes usable;
4. never by shrinking the target's context, cache or batch - rule 35.

## The measurement that would confirm the calculation

One pair of runs at a small context, where both start: the control, and `draft-mtp`, reading VRAM from each. The delta is
the head's real footprint, and it should be near the 6 GiB the arithmetic predicts at 262 144, scaled down.

## State

| question | answer |
|---|---|
| does the head share the target's memory today | **no**, for qwen35moe |
| is that required by the hardware | no |
| what is the largest single cost | a second KV, ~6 GiB at the accepted context, by calculation |
| what is measured so far | that it fails at the accepted profile, and at which allocations |
