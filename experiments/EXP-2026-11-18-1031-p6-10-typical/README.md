# EXP-2026-11-18-1031 — P6-10: Typical-P on Goose, differential 1800/1800

## Blocker found first

`goose/sampling/sampler-chain-full.goose` as delivered does not compile with
either qualified compiler (`8b7a56bb…` and `.cache/goose/simd7`):
`src/chain.goose:39: error: expected top-level declaration` at `export fn
sm_mt_seed`. The `export` keyword is not accepted in a module, so the delivered
chain cannot be linked to the shipped library in this form. The working copy
here replaces `export fn` with `fn` and adds a gate entry; the algorithms are
untouched.

## Typical-P

`sm_apply_typical` follows `llama_sampler_typical_apply` exactly: sort by logit
descending, softmax, entropy, shifted = |(-log p) - entropy|, order by shifted
ascending, cumulative until `> typical_p` with `i >= min_keep-1`, kept set left in
shifted order. The sort is a selection sort (O(n^2)); a heap sort is required
before shipping for the full vocabulary — recorded, not hidden.

## Differential result

`src/diff.cpp` compares the Goose function with the native algorithm transcribed
with `std::sort`, 200 random distributions x (tp 0.5/0.9/0.99) x (min_keep 0/1/5):

```
typical-p: cases=1800 mismatches=0
```

Sizes and kept token ids match in every case.

## Heap sort attempt (reverted)

The first heap-sort attempt negated the shifted scores before a max-heap
extraction, which produced the reverse order and failed every case; it was
reverted. The corrected version builds the max-heap directly on the shifted
scores and extracts to the tail, giving ascending order with lv, id and p
moving together. It passes the same gate unchanged: `cases=1800 mismatches=0`.
Typical-P therefore runs at O(n log n).

## Unsorted Top-P attempt (reverted)

An unsorted top-p was added (softmax without a sort precondition, ascending
heap sort, cumulative read from the tail, kept suffix reversed to descending).
It failed 747 of 900 cases, returning a constant 5-token cut where the native
algorithm cut at 2, which points at the probability array not being aligned with
the sorted order rather than at the cut rule. Reverted, not shipped. The
validated typical-p (1800/1800) remains the state of this card.

## Unsorted Top-P, second attempt (reverted, sharper diagnosis)

Rewriting the filter so the probabilities are recomputed from the sorted logits
(no second array to fall out of alignment) produced the identical failure:
747/900, constant cut at 5, and equal-count cases still mismatching in the kept
ids. Since the probability path was removed as a suspect, the remaining cause is
the order/semantics of `sm_partial_sort` (the module's top-k sort) when it is
used as a full descending sort: its comparator or its placement of results does
not match `std::sort` descending. The next attempt must verify the primitive on
a fixed array before the filter is rebuilt on it.

## sm_partial_sort verified (probe)

Fixed array `lv=[3,1,2,5,4]`, `id=[0,1,2,3,4]`, called as a full sort:

```
ret=5 lv: 5.0 4.0 3.0 2.0 1.0  id: 3 4 0 2 1
```

The primitive sorts descending and moves ids in step, so the earlier diagnosis
that blamed it is **wrong**. The unsorted top-p failure therefore lies in the
filter code or its argument mapping, not in this sort. The probe stays in the
card (`gate_sort`) so the next attempt starts from a verified primitive.

## Unsorted Top-P: minimal probe exposes the defect (not accepted)

Fixed array `lv=[1,2,3,4,5]`, `id=[0,1,2,3,4]`, `top_p=0.5`, `min_keep=1`; the
native cut is 1 with id 4 (top probability 0.6364 >= 0.5). The Goose path:

```
cut=2 ids: 3 4   lv after call: 0.5 0.5 1.5 2.5 3.5
```

Root cause found: the C wrapper called the wrong generated function, so both
gates executed typical-p (identical output, and the shifted scores looked like a
corrupted array). The fix is a single exported dispatch with an explicit op code
(`gate_dispatch(op, ...)`), which removes index guessing entirely.

Accepted now: unsorted top-p (small-candidate path, n <= 1024) matches the native
filter in **900/900** cases (cut and kept ids), and typical-p still matches in
**1800/1800**. The fixed-array probe through the dispatch gives the native
result: `cut=1 id=4, lv=5,4,3,2,1`.

## Unsorted Top-P validated on both size regimes

The native filter uses a partial sort with k=256 and extends k to the full size
when the cumulative mass has not reached p; a full sort yields the same prefix,
so the Goose filter sorts fully and the result is identical. Gate:

```
typical-p:      cases=1800 mismatches=0
top-p unsorted: cases=900  mismatches=0   (n = 64)
top-p large:    cases=45   mismatches=0   (n = 2048)
```

Cut and kept ids match the native filter in every case, including the large
vocabulary where the native heuristic is active.

## XTC accepted

Transcribed from `llama_sample_xtc_apply`: guard on probability, threshold and
size; the caller draws the uniform value and passes it in (`chance`), so the
filter stays pure while the RNG stream remains a separate port; softmax over the
sorted candidates; find the last index with `p >= threshold`; drop that prefix
when a tail of at least `min_keep` survives. The dispatch carries the extra
parameters (`f2` = threshold, `f3` = chance).

```
xtc: cases=720 mismatches=0
```

60 distributions x threshold {0.1, 0.3} x min_keep {1, 3} x chance {0.0, 0.4,
0.9}; the cut and the surviving token ids match native in every case, including
the no-op branch when chance exceeds the probability.

## DRY, step 1 accepted (restart-sequence limit)

`sm_dry_rep_limit` is the first step of `llama_sampler_dry_apply`: scan the
context backwards, find the nearest restart sequence, and return the repetition
window limit (`rep_limit`). Breakers are passed as flattened arrays (head, tail
start, tail length, tail tokens), so no container is needed in Goose. Indices
use `rat(i) = last[n-1-i]` exactly as the native card ring buffer.

```
dry step1: cases=200 mismatches=0
```

200 random contexts (n=32, vocabulary 8) with 1-5 breakers whose tails are 0-2
tokens long; the results match the native loop, including multi-token tails and
the no-match case. Steps 2 (Z-algorithm repeat counts) and 3 (penalty
application) are not ported yet.

## DRY step 2 accepted (reverse Z-algorithm repeat counts)

`sm_dry_repeat_counts` is the second step of the native filter: the reverse
Z-algorithm fills one repeat count per position, clamped to `rep_limit`.

```
example: 0 0 3 1 0 2 0 0 0 0   (the array documented in llama-sampler.cpp) PASS
dry step2: cases=300 mismatches=0
```

The documented example matches element for element and 300 random contexts
(n=32, vocabulary 5, random rep_limit) match the native algorithm. Step 3
(turning the counts into a penalty over the candidate logits) remains open.

## DRY steps 3-4 accepted (penalty application)

`sm_apply_dry` turns the repeat counts into logit penalties. The maximum repeat
for a candidate is found by scanning the context instead of the native per-token
map, so no container is needed; single-token breakers are skipped as in the
native driver. `logf`/`powf` are declared as externs and the exponent is clamped
by a float comparison instead of an integer truncation, which is equivalent for
practical exponents.

```
dry step3: cases=100 mismatches=0    (logits compared bitwise)
```

100 random contexts with random repeat counts, two breakers (one single-token),
random base candidates: the resulting logits are bit-identical to the native
formula `multiplier * pow(base, repeat_exp)`. DRY is now complete in Goose:
steps 1, 2 and 3-4 are all gated.

## Adaptive-P accepted

`sm_adaptive_target` reproduces the clamped EMA target and `sm_apply_adaptive_p`
the logit transform `5 - 10*d^2/(1+d)` with `d = |(p - adapted) * (1/0.3)|`,
skipping `-inf` logits, followed by the same unsorted softmax.

```
adaptive-p: cases=200 mismatches=0   target mismatches=0
```

Both the adapted target and the resulting probabilities are bit-identical to the
native computation over 200 random distributions.

## Generated function ids are now auto-detected

Hardcoded `gg_function_N` ids broke when the module grew: two wrappers silently
called a different function, which is what produced the earlier confusing top-p
failure. `gen_ids.py` now reads the dispatch prototype out of `build_chain.c` and
writes `src/fn_ids.h`; `src/wrap.c` includes it, so the id can never be stale.
The wrapper compiles with `-Dmain=fg_chain_main`.

## Mirostat v1 decision logic accepted

`sm_mirostat_k` estimates `s_hat` from the top m probabilities, then computes
`k = pow((eps * 2^mu) / (1 - n_vocab^-eps), 1/s_hat)` and truncates it to at
least 1 (the loop is capped so a non-finite k cannot spin). `sm_mirostat_update`
applies `mu -= eta * (observed_surprise - tau)`.

```
mirostat: k mismatches=0  mu mismatches=0   (200 random distributions)
```

The truncation size and the updated mu are identical to the native formulas.
What remains outside Goose here is only the draw itself (the RNG stream, a
separate port) and the top-k truncation, which uses the already verified sort.

## Module globals must be initialised by the host

`gg_init_globals()` is called by the generated `main`, not by the module's
functions. A host that embeds the module (this gate, and any future shipped
integration) must call it: without it every module constant reads as 0, so
`x & M32` was always 0 and the MT stream returned zeros. `src/wrap.c` now runs
it from a constructor.

## MT19937 does not yet match std::mt19937

With the globals fixed, the module's generator produces real values but they do
not match the standard generator:

```
seed 1, draw 0 equal, draw 1: ref=3093770124 goose=4282876139
                              ref=491263     goose=3093770124 (one step late)
rng: cases=240 stream mismatches=220
```

The probe compares 20 seeds x 12 draws against `std::mt19937` and also checks
that `(float)u / 2^32` reproduces `uniform_real_distribution<float>(0,1)`. The
second check cannot pass until the stream matches, so the RNG port is **not**
accepted: the module's seeding/twist needs a fix, and the verification harness
is kept here for it.

## RNG accepted (the earlier mismatch was in my harness)

The comparison drew two values per iteration from the reference generator, so
streams were compared out of phase and the float check compared different
numbers. With one draw per iteration and the same single draw for both formulas:

```
rng: cases=1280 stream mismatches=0 float mismatches=0
```

20 seeds x 64 draws: the module's stream equals `std::mt19937` exactly, and
`(float)u / 4294967296.0f` reproduces `uniform_real_distribution<float>(0,1)` bit
for bit. With `gg_init_globals` called, the RNG port is complete; a fixed seed
therefore produces the same draws in Goose as in native.

This is the second time a defect was in my own harness rather than in the
module. The lesson is recorded: compare one draw to its counterpart, never mix
consumers inside a comparison loop.

## Filter order, read from the shipped chain builder

`common/sampling.cpp` adds the filters in this order, and a Goose chain runner
must follow it exactly:

```
dry -> top_k -> top_p -> min_p -> xtc -> typical -> temp_ext -> penalties
    -> adaptive_p (when selected) -> greedy | dist
```

Against that order the module currently holds:

| step | Goose | evidence |
|---|---|---|
| dry | yes (steps 1-4) | 200/200, 300/300 + documented example, 100/100 bitwise |
| top_k | not ported (the verified sort exists) | - |
| top_p | yes (unsorted, both regimes) | 900/900, 45/45 |
| min_p | not ported | - |
| xtc | yes | 720/720 |
| typical | yes | 1800/1800 |
| temp_ext | not ported | - |
| penalties | not ported | - |
| adaptive_p | yes | 200/200 bitwise |
| dist | not ported as a sampler (the stream is verified) | 1280/1280 |

So a full-chain runner must not be assembled yet: five steps would be missing
and the order would be wrong if typical were applied before top_p. The next step
is top_k and min_p on the verified sort, then temp_ext and penalties, then the
runner, then the fixed-seed chain test.

## top-k and min-p accepted

`sm_apply_top_k` sorts fully and keeps the k best in descending order; the
module's partial sort does not order the kept prefix, which top-k requires, so
the full sort is used and only the size is cut. `sm_apply_min_p` follows the
unsorted native form: threshold on the logit (`max + log(p)`), count first, and
compact in the original order only when the count is non-zero and at least
`min_keep`.

```
topk: mismatches=0   min-p: mismatches=0
```

200 distributions for top-k with random k, and 200 x 3 thresholds x 2 min_keep
for min-p: the kept set, its order and the resulting size all match.

Two measurements in this step were mine, not the module's: the logits were drawn
from only 2000 distinct values, so ties were frequent and the harness compared
tie order against a different sort. With distinct logits the comparison is
clean. Tie order is unspecified in the native sort as well.

## dist sampling: not accepted yet

`sm_sample_dist` reproduces the libstdc++ shape (normalise by the weight sum,
prefix sums, `lower_bound` of a `generate_canonical<double,53>` draw) and the
gate compares it with a real `std::discrete_distribution` fed by the same
`std::mt19937` seed:

```
dist: cases=400 mismatches=368   (goose almost always returns index 0)
```

Returning index 0 in ~92% of the cases means the draw `u` is effectively zero,
which points at `mt_canonical` (or the i64 to f64 conversion inside it) rather
than at the prefix walk: `sm_mt_next` itself is verified against `std::mt19937`
on 1280 draws. The next attempt must gate `mt_canonical` on its own against a
`generate_canonical<double,53>` reference before touching the selection.

## mt_canonical verified; the defect is inside the selection

`mt_canonical` matches `generate_canonical<double,53>` exactly, checked by
scaling it by 2^32 and comparing with the second 32-bit word of the same pair:

```
seed 1 pair (1791095845, 4282876139): expect 4282876139.417  goose 4282876139
seed 2 pair (1872583848,  794921487): expect  794921487.436  goose  794921487
seed 3 pair (2365658986,  303761048): expect  303761048.551  goose  303761048
```

So the draw is correct and the zero-index result comes from inside
`sm_sample_dist`: either the `f32` to `f64` conversion of the weights or the
prefix walk. The next attempt must print the computed `u` and the prefix values
for one fixed case (n=8, weights 1..8, seed 1) instead of another full sweep.

## The draw is lost only when stored in a local f64

Single fixed case (n=8, weights 1..8, seed 1), both branches in the same
dispatch, same `mt_canonical(id)` call:

```
op 15: (mt_canonical(id) * 2^32) as i64   -> 4282876139   (correct for seed 1)
op 16: let du: f64 = mt_canonical(id);    -> p[0] = du as f32 prints 0
        selection then returns index 0
```

The prefixes in the same op 16 are correct (0.0833, 0.1667, 0.2778, 0.4167,
0.5833, 0.7778, 1.0), so only the draw is lost. Using the value inside an
expression keeps it; assigning it to a local `f64` and comparing it later yields
zero. That is a codegen defect, not a sampling one, and it is the real reason the
dist gate failed while `sm_mt_next` and `mt_canonical` both measured correct.

Minimal repro to hand to the compiler: a function returning `f64` built from two
i64 calls, stored in a local, then compared/printed - the stored value is 0.

## Blocker: draws from inside the dispatch do not reach the comparison

Three attempts on the same blocker, all with a fixed case (n=8, weights 1..8,
seed 1):

1. `sm_sample_dist` with `mt_canonical(id)` stored in a local `f64` -> the draw
   reads as 0 and the selection returns index 0.
2. The same call used inside an expression (op 15) -> correct value
   4282876139 for seed 1. So the value survives only inside an expression.
3. A rewrite that keeps the two 32-bit words as `i64` and compares in one
   expression (op 17) -> still selects index 0, which means the words read as 0
   inside that branch even though `sm_mt_next` is verified correct when called
   directly (op 8, 1280 draws) and the prefix arithmetic in the same function is
   correct (op 16 printed exact prefixes).

What is established: the MT stream is correct, `mt_canonical` is correct in an
expression, the weight arithmetic is correct, and the draw is lost when the
selection function runs inside the dispatch branch that also writes to another
slice. What is not established: whether this is storage of computed f64, slice
aliasing between the state slice and the output slice, or ordering of the draw
relative to the loop.

Per the repository rule, trial and error stops here: this needs the senior
helper's diagnosis rather than a fourth variant.
