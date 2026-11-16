# EXP-2026-11-15-749 — it is accumulated floating-point drift, not a discrete defect

Base: `303a6ad5`. This card corrects my own methodology and, with it, the conclusion of EXP-747 and
EXP-748.

## 1. The methodological error, first

Two of my harness scripts were used interchangeably and **did not send the same request**:

- `greedy.sh` posted `req_greedy.json` - a 32768-token prompt;
- `logit.sh` posted `req_8k.json` - an 8192-token prompt - because a `sed` substitution had silently
  failed and the file path in the script was never what I thought it was.

Every result I reported as "24 tokens" and as "32K tokens" from `logit.sh` was therefore **the same
8192-token run**. That is why the delta looked constant at 4e-5 "at both lengths": it was one length,
measured once.

The authoritative field is `timings.prompt_n` in each response, and it was in every JSON I already had.
I checked the request files instead of the response, which is the same class of mistake as storing the
answer hash and never comparing it.

## 2. Re-measured with one script, prompt length read from the response

| prompt | b4096 token 1 logprob | b8192 token 1 logprob | delta | argmax |
|---:|---:|---:|---:|---|
| 64 | -1.335e-05 | -1.335e-05 | **0** | same (bit-identical) |
| 8192 | -0.079990 | -0.051140 | 2.9e-02 | same |
| 16384 | -0.051431 | -0.000144 | 5.1e-02 | same |
| 32768 | -0.779915 | -0.741721 | - | **flips** |

Both 16K arms repeated exactly, so the measurements are deterministic and the deltas are real.

## 3. What the corrected picture says

**It is not a constant 4e-5 shift. The drift grows with prompt length** - zero at 64 tokens, 2.9e-2 at
8K, 5.1e-2 at 16K - and at 32K it crosses a near-tie and flips the argmax.

That is the signature of **accumulated floating-point drift**, not of a discrete wrong code path:

- no difference at all when the batch is small (bit-identical, not merely close);
- small, growing drift as the prefill is split into more chunks;
- a near-tie at 32K that flips because the drift reached it.

The "discrete clusters" I reported in EXP-748 are explained by the same thing: `-b 8192` and `-ub 3072`
agreed for 12 tokens because both had drift in the same direction, then diverged when a near-tie fell
between them. They were not two code paths.

## 4. Consequences

**Retracted:** the claim that a batch-dependent code path produces materially different logits, and the
"small number of discrete computational paths" model.

**Stands, and is now better supported:** configurations other than `-b 4096 -ub 4096` do **not**
reproduce the reference answer at 32K. That remains true regardless of mechanism, and it is still the
criterion for a reference-equivalent configuration.

**Also stands:** the cache never changes an answer; `max_batch = 32` bypasses the expert cache during
any prefill.

## 5. What this does not settle

Whether the drift is acceptable. A 5e-2 logprob difference is much larger than f32 reordering (~1e-7)
should produce, so *something* amplifies it - but "amplified drift" and "defect" are now distinguishable
only by measuring intermediates, not by inference from outputs.

And the practical question the project actually cares about: at the reference shape the answer matches
the recorded hash, so the reference configuration is usable. The alternative shapes are not, and the
reason is drift plus near-ties rather than a clear bug.

## 6. Procedural fix, so this cannot recur

Every harness script must now print `timings.prompt_n` and the request file it posted into its own log,
and a comparison script must refuse to compare two runs whose `prompt_n` differ. That is a check on the
tooling, not on the model - and it would have caught both of this session's methodological errors (this
one, and the uncompared answer hash).

## 7. Cost

Ten server runs across four prompt lengths.
