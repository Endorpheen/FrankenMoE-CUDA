# EXP-2026-11-15-748 — both `-b` and `-ub` change numerics; the wrong configs form clusters

Base: `434af0bd`. Narrowing where the batch parameters enter the computation.

## 1. `-b` changes the answer with `-ub` untouched

At a fixed `-ub 4096` and cache 4352, 32K prompt, 512 predicted tokens:

| `-b` | answer sha | matches reference? |
|---:|---|---|
| 4096 | `c2ff42cc` | **yes** |
| 8192 | `dc2d651b` | no |

And it is not a sampling artefact - greedy (temp 0):

| `-b` | first tokens | sha |
|---:|---|---|
| 4096 | 11, **198**, 248069, 271, 2, 5194 | `39cff5a2` |
| 8192 | 11, **1716**, 58558, 13, 198, 760 | `aba0318b` |

Divergence at token 1.

## 2. The size of the first-token difference is tiny and constant

| prompt | `-b 4096` logprob | `-b 8192` logprob | delta |
|---:|---:|---:|---:|
| 24 tokens | -8.607e-05 | -4.637e-05 | 4.0e-05 |
| 32K tokens | -8.607e-05 | -4.637e-05 | 4.0e-05 |

**Exactly the same 4e-5 delta at both lengths.** So `-b` perturbs the first token by a fixed, small
amount that does not accumulate with prompt length - and yet the 512-token answer is completely
different. The divergence is therefore **amplified during decoding**, not created during prefill.

That finally explains why the chunking hypothesis in EXP-746 looked right: long prompts made the
divergence visible, but the prompt length is not what causes it.

## 3. The wrong configurations form clusters, not noise

Greedy output token sequences:

| configuration | first 8 tokens |
|---|---|
| `-b 4096 -ub 4096` (reference) | 11, 198, 248069, 271, 2, 5194, 2743, 35059 |
| `-b 8192 -ub 4096` | 11, 1716, 58558, 13, 198, 760, 6951, 26817 |
| `-b 4096 -ub 3072` | 11, 1716, 58558, 13, 198, 760, 6951, 26817 |
| `-b 4096 -ub 2048` | 11, 1156, 579, 1622, 369, 310, 8290, 264 |

**`-b 8192` and `-ub 3072` produce identical tokens for the first 12, then diverge.** Two different
knobs, same output for 12 tokens. That is not random perturbation - it means both configurations fall
into the same alternative code path, and the paths differ from each other only later.

So there are a **small number of distinct computational paths**, not a continuum of rounding:
the reference path, and at least two others.

## 4. What the source says

`runtime/reconstructed/src/llama-moe-cache.cpp`:

```cpp
static constexpr int64_t max_batch = 32;

ggml_tensor * get_slot_map(int32_t il, int64_t n_tokens, int64_t n_expert_used) const {
    ...
    // large batches use most experts of a layer, so they gain little from the cache
    // and would evict the experts used in generation
    if (n_tokens == 0 || n_tokens > max_batch || ... > groups[l.ig].n_slots) {
        return nullptr;
    }
```

`max_batch = 32`, so during any prefill (>= 2048 tokens) the expert cache is **bypassed** in every
configuration tested. The cache is not the discriminator - which the data confirms, since cache size
never changed a hash.

That leaves the expert gather and the recurrent state update as the batch-dependent paths, consistent
with the model being 30 recurrent layers of 41.

## 5. Status

The mechanism is not yet identified, but the search space is now small and well-constrained:

- not the expert cache (size never changed a hash; bypassed during prefill anyway);
- not summation order (delta would be 1e-7, is 4e-5, and greedy diverges);
- not prefill chunking (same delta at 24 and 32K tokens);
- **is** something that depends on the effective batch shape, produces a fixed 4e-5 first-token
  perturbation, and comes in a small number of discrete variants.

Next step: instrument one decode step at `-b 4096` and `-b 8192` and compare intermediate tensors,
rather than continuing to infer from outputs.

## 6. Cost

Six server runs across the two parameters.
