# EXP-2026-11-15-743 — `-ub` changes the model's numerics, and this invalidates the speed comparison

Base: `3fa749ba`. Checking the correctness of the new configurations turned up something that matters
more than any of the speed numbers.

## 1. The finding

Answer hashes, same prompt, same seed, same temperature:

| configuration | cache | answer sha |
|---|---:|---|
| ub 4096 (reference) | 4352 | `c2ff42cc` **= the historical reference** |
| ub 4096 | 4608 | `c2ff42cc` |
| ub 3072 | 5120 | `1fa8f77c` |
| ub 3072 | 5376 | `1fa8f77c` |
| ub 2048 | 5120 | `8c50dd2d` |
| ub 2048 | 5376 | `8c50dd2d` |

**The expert cache does not change the answer. The batch size does.** Three ubatch values produce three
stable, different answers.

## 2. It is not rounding

Token-by-token comparison against the reference:

```
token 0: both arms emit "," with logprob -0.0001   -> identical computation
token 1: ub 3072 emits " check" with logprob -0.0128
         ub 4096 emits "\n"     with logprob -0.7799
```

A token at logprob -0.78 cannot beat one at -0.013 under any sampling of the *same* distribution. So
**the logits themselves differ**, and they differ from the second token onward.

## 3. What this means for everything measured today

Every decode figure in EXP-733 through EXP-742 was measured with a configuration that produces a
**different answer** than the reference. Those numbers are still real measurements of those
configurations' speed - but they are not a faster way to compute the reference answer. They are a
faster way to compute a different one.

That is exactly the distinction this project has enforced elsewhere: EXP-721 rejected KV `f16` for
changing the answer at +1.2%, and the same standard applies here at +2.6%. Applying the rule
consistently means:

- **ub 4096 remains the only configuration that reproduces the reference answer.** Its best measured
  speed is 40.99 at 512 tokens (cache 4608) - above the reference 41.94? No: below. At cache 4352 it is
  39.86.
- **ub 3072 (43.05) and ub 2048 (42.35) are excluded as reference-equivalent configurations**, however
  fast they are, until the cause of the numeric difference is understood and removed.

I had recorded those configurations as recommended defaults. That was wrong, and this card retracts it.

## 4. Why the batch size changes logits at all

Not yet established. Candidates:

1. **Batch-dependent kernels.** The CPU MoE path may choose different kernels per ubatch size
   (`GGML_GOOSE_TILED`, `MMQ` dispatch, tile counts), and different kernels sum in different orders.
2. **A real defect in the smaller-batch path.** If the difference is larger than reordering can explain,
   something is wrong - and a 0.77-logprob gap between candidate tokens is large.

Distinguishing them is measurable: a batch-size sweep with the same seed and a per-token probability
comparison would show whether the divergence is at the 1e-6 level (reordering) or the 1e-1 level (a
defect). The single sample above suggests the latter, but one token is not evidence of a mechanism.

## 5. What this retracts

- `run-apex.sh` default `--ub 2048`. **Reverting to the reference argv** until the cause is known.
- `BEST-CONFIG.json` naming ub 2048 / ub 3072 as recommended. **Marked as not reference-equivalent.**
- The claim in EXP-740 that the configuration "beats the reference at all three lengths". It beats it on
  speed while producing a different output.

## 6. What stands

The measurements, the cache threshold at 5376, the frontier shape, the packaging fixes, the profile
analysis. Speed conclusions about ub 4096 remain valid and it is the only reference-equivalent option.
