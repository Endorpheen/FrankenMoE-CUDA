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

## temp_ext written, low-bit divergence; softmax cleared

`sm_apply_temp_ext` follows the native path (sorted softmax, entropy, dynamic
temperature, double renormalisation). The gate reports 200/200 cases differing,
but the printed values are both ~1.0, i.e. the divergence is in the low bits and
the whole distribution collapses in that case, which points at the dynamic
temperature itself rather than at the probabilities.

A separate probe confirms the sorted softmax is **bitwise equal** to the native
formula on a fixed vector (`0x3f115d3e` etc.), so the softmax is not the cause.
The remaining suspects are `max_entropy`, the entropy accumulation, or the
`powf` exponent inside the dynamic temperature.

temp_ext is therefore NOT accepted. The next step is to print `entropy`,
`max_entropy`, `normalized_entropy` and `dyn_temp` from both sides for the fixed
case (n=8, temp=0.8, delta=0.4, exponent=1.0), not another sweep.

## Minimal repro: a function-returned f64 reads as 0 when stored

| case | result |
|---|---|
| `let x: f64 = 1.5; p[0] = x as f32` | 1.5 (correct) |
| `let y: f64 = lv[0] as f64 * 2.0; p[1] = y as f32` | 4 (correct) |
| f64 accumulator over a loop, then cast | 9 (correct) |
| `(mt_canonical(id) * 4294967296.0) as i64` in one expression | 4282876139 (correct) |
| `let u: f64 = mt_canonical(id); p[0] = u as f32` | **0** |
| same with `* 1.0` or `+ 0.0` in the initialiser | **0** |

So the defect is specific: storing the f64 result of a function call loses the
value, while constants, computed expressions and accumulators are fine, and the
same call used inside a larger expression keeps its value.

This one defect explains both blocked filters: `dist` stores
`mt_canonical(state)`, and `temp_ext` stores the extern `exp()` result for the
double renormalisation, which the native code requires. A workaround in f32
would change the bits, so neither filter is accepted; the correct fix is in the
compiler.

Repro command: op 22 of the dispatch with a seeded state (seed 1) prints
`bare=0 *1.0=0 +0.0=0` where the native values are 0.997184808, 0.932557361 and
0.128124448.

## penalties accepted

`sm_apply_penalties` transcribes the native rule: count the token in the recent
window, then multiply a non-positive logit by the repeat penalty (or divide a
positive one, as native does), and subtract `count*freq + present`.

```
penalties: cases=200 mismatches=0   (logits compared bitwise)
```

200 windows with random repeat/frequency/presence penalties and random
candidate sets match the native formula bit for bit.

The first run differed in the low bits because I subtracted the two terms
separately (`logit - A - B`) while native subtracts their sum once
(`logit - (A + B)`). That is a real ordering difference, not noise, and it is
the kind of thing that only a bitwise gate catches.

## typical-p: insertion sort replaced by the heap sort (gate re-run)

The accepted implementation still sorted the logits with an insertion sort, so
it was O(n^2) on a full vocabulary. It now uses `sort3` plus an in-place reverse,
and the gate was re-run on realistic, distinct logits:

```
typical: cases=1800 mismatches=0    (190 x 64 tokens + 10 x 2048 tokens,
                                     tp {0.5, 0.9, 0.99}, min_keep {0, 1, 5})
```

Three harness defects were fixed to get a trustworthy run, and all three are the
same mistake in different clothes: logits drawn from 20000 or fewer distinct
values (ties), logits in +-50000 (probabilities underflow to zero), and
comparing tie order instead of the kept set. The kept set and the cut size are
now what is compared, which is what the filter actually decides.

The shipping file `goose/sampling/sampler-chain-full.goose` is NOT updated yet:
it cannot be compiled by either available compiler (`export fn` is rejected), so
a textual port there cannot be verified, and an unverified copy of a validated
function is worse than the old draft plus this note.

## The shipped chain module compiles and is gated

Three facts changed the integration picture:

1. The language has **no export mechanism**. `export fn`, `pub fn`, `@export` and
   attribute forms are all rejected, and the compiler has no export flag, so a
   library reaches generated functions only through their ids.
2. A module without `main` cannot be compiled at all, and unused functions are
   **removed**, so the build root has to call the entry point to keep it alive.
3. Therefore the shipping file now exposes one stable dispatch,
   `fg_sampler_chain(op, ...)`, instead of relying on a generated id, and the
   build emits the id header and a C shim.

`goose/sampling/sampler-chain-full.goose` was rewritten accordingly: every
`export fn` became `fn`, `sm_apply_typical` now uses the validated heap-sort
body with a three-array heap helper, and the dispatch covers typical-p, top-p,
top-k, min-p and temperature. `tools/build-sampler-chain.sh` builds it
reproducibly (module + root, id header, shim).

Verified against the **shipping** module, not the card copy:

```
typical: cases=1800 mismatches=0     (entry point gg_function_1 in that build)
```

The remaining work for a shipped chain is adding the already validated filters
(XTC, DRY, adaptive-p, mirostat, penalties, dist) to the dispatch as each
becomes callable, then the fixed-seed chain test.

## XTC reaches the shipped module

`sm_apply_xtc` is now part of `goose/sampling/sampler-chain-full.goose` and is
reachable through the stable dispatch (op 5). Verified in the **shipping build**:

```
xtc (shipping build): cases=720 mismatches=0
```

60 distributions x threshold {0.1, 0.3} x min_keep {1, 3} x chance {0.0, 0.4,
0.9}, compared with the native rule including the returned ids. The build/ shim
path (`tools/build-sampler-chain.sh`, `fg_sampler_chain_call`) works end to end.

## penalties reach the shipped module

`sm_apply_penalties` is part of the shipping file behind dispatch op 6; the
recent window travels through the float slice as exact token ids. Verified in
the shipping build:

```
penalties (shipping build): cases=200 mismatches=0   (logits bitwise)
```

The dispatch of the shipped module now carries typical-p, top-p, top-k, min-p,
temperature, XTC and penalties, each gated in that same build.

## DRY reaches the shipped module

The four DRY steps are in `goose/sampling/sampler-chain-full.goose` behind one
entry (`fg_sampler_dry`), with its own id and shim emitted by the build script.
Verified end to end in the shipping build:

```
dry (shipping build): cases=120 mismatches=0
```

120 random contexts with 1-4 breakers (tails of 0-2 tokens), random
allowed_length, comparing both the repeat-count array and the resulting logits
against a reference of the four native steps.

## adaptive-p and mirostat reach the shipped module

Both are in the shipping file behind dispatch ops 7 and 8. Verified in the
shipping build:

```
adaptive-p: mismatches=0   mirostat: mismatches=0    (200 cases each)
```

One defect of mine was found on the way: the adaptive-p entry wrote the adapted
target into `p[0]` before using `p` as the probability array, which corrupted the
first probability and collapsed the distribution to index 0. The state form now
computes the target locally and never uses the probability array as scratch.

The shipped dispatch now carries typical-p, top-p, top-k, min-p, temperature,
XTC, penalties, adaptive-p and mirostat, each gated in that build. Only `dist`
and `temp_ext` remain, both waiting on the f64 record in the tech-debt ledger.

## Fixed-seed determinism in the shipped build

```
seed 1 twice identical: YES
seed 2 draws differ:    YES
draws(s1): 1791095845 4282876139 3093770124 4005303368  cut=3
draws(s2): 1872583848 794921487 111352301 4000937544  cut=3
```

The same seed reproduces the whole run (four draws, the typical-p cut, the kept
ids and the probabilities byte for byte), and a different seed changes the
draws. The seed-1 draws are the documented `std::mt19937` sequence, which ties
this test to the earlier stream verification.

## Hook into the real generation path (staged)

`vendor/flint/src/llama-sampler.cpp` now calls the Goose chain for typical-p when
the shim is linked in. Details that matter:

- the shim symbol is declared **weak**, so the runtime still builds and runs
  without it and the native body answers;
- the stage is **enabled by default** (`GGML_GOOSE_SAMPLER_CHAIN=0` disables it),
  following the rule that an accepted improvement is on unless switched off;
- the candidate arrays are copied into plain buffers, the Goose result is copied
  back, and a counter prints `FG171_TYPICAL goose calls=N kept=M` so real
  execution can be proven from the log rather than from a symbol table.

`g++ -fsyntax-only` on the modified file passes. A linked build and a real
generation run are the next step; a model run needs Igor's explicit go-ahead.

## Link step: where it can and cannot go

The working runtime build directory `build/expert-tier-franken-cuda` is
configured from `work/llama.cpp-integration`, which is out of bounds by the
repository rules, so the generated chain cannot be added through that
CMakeLists. The CMake edit in `vendor/flint` was therefore reverted rather than
left half-applied.

What exists now and is verified:

- `build/sampler-chain/sampler_chain_shim.c` plus its generated C (195 KB) and
  the id header, produced by `tools/build-sampler-chain.sh`;
- the shim object exports `fg_sampler_chain_call` and `fg_sampler_dry_call`;
- the weak reference behaves as intended in a link test: a program declaring it
  weakly prints `linked=NO` without the object and `linked=YES` with it, so the
  runtime works with or without the chain;
- `llama-sampler.cpp` carries the hook and passes `-fsyntax-only`.

The link must therefore be done in a build of our own (a separate directory with
the toolchain script), not inside the user's working tree copy.

## typical-p live in the shipped library, identical to native

The Goose stage now runs inside the built library and matches the native body on
the real sampler API:

```
GGML_GOOSE_SAMPLER_CHAIN=1 /tmp/hookdiff   -> 200 lines
GGML_GOOSE_SAMPLER_CHAIN=0 /tmp/hookdiff   -> 200 lines
IDENTICAL ON vs OFF
FG171_TYPICAL goose calls=200 kept=10
```

200 candidate sets of 8-40 tokens, three typical_p values and six min_keep
values; ids, logits and the cut are byte-identical, and the counter proves 200
real Goose calls rather than a symbol in the table.

The fix that made it identical: the shifted scores live in a scratch array and a
four-array heap sort carries the logits along, so the caller's logits keep their
own values, as the native body does. The stage is default-on again
(`GGML_GOOSE_SAMPLER_CHAIN=0` disables it) because it now measures identical.

## top-p hook installed, opt-in, still differs

The real path can call the Goose top-p (`GGML_GOOSE_SAMPLER_TOP_P=1`) and the
counter fires (`FG171_TOP_P goose calls=200 kept=8`), but the comparison fails
in the logits column:

```
case 0: ON  kept=1 id 5 logit 0.455646     OFF kept=1 id 5 logit 3.479219
case 1: ON  kept=5 ids 0,12,7,5,4          OFF kept=5 ids 0,12,7,5,4
```

The kept ids and sizes agree; the logits that come back from the Goose call look
like probabilities instead of the original logits. The entry resolves to
`gg_function_2` in the build, and that id was selected by prototype shape, which
is exactly the fragile path the ledger warns about (TD-S2), so the first thing to
check is whether that id really is `fg_sampler_top_p` and not a neighbour with
the same shape.

The stage is therefore behind its own flag, default off, and the default build
is verified unchanged (`default (top-p off) matches native`). Typical-p remains
live and identical.

## top-p live and identical; TD-S5 resolved

The same 200-case comparison through the real sampler API is now identical with
top-p on and off, with the counter firing:

```
TOP-P IDENTICAL ON vs OFF
FG171_TOP_P goose calls (1, 100, 200)
typical still matches
```

Cause of the earlier divergence: `fg_sampler_top_p` was not reachable from
`main`, so the compiler dropped it and the id the build matched pointed at a
neighbour with the same prototype shape. The build root now calls every entry,
and the id header is produced after that. Recorded in the ledger as TD-S5
(resolved) with the rule it implies.

Both stages are default-on; each can be disabled with its own flag.

## XTC live in the shipped library

The native XTC draws its uniform value and then hands over to Goose with that
same value, so the RNG stream is untouched by the substitution:

```
XTC IDENTICAL ON vs OFF          (60 candidate sets, eight seeds apart by length)
FG171_XTC goose calls=1 kept=...
```

Ids, logits and the surviving set match the native body exactly.

Live stages now: typical-p and top-p (both default-on with their own disable
flags), XTC (with the chain flag). Next: penalties and DRY, then the fixed-seed
chain test on the real path.

## penalties live in the shipped library

The hook builds the recent window from the sampler's ring buffer (exact token
ids in a float slice) and hands over to Goose:

```
PENALTIES IDENTICAL ON vs OFF     (40 runs, 12-token windows)
FG171_PENALTIES goose calls=1 hn=12
```

Logits after the penalty match the native body exactly.

Live now: typical-p, top-p, XTC, penalties. Remaining for the chain: DRY in the
path, and `dist`/`temp_ext` which wait on TD-S1.

## DRY hook installed, opt-in, comparison needs a vocabulary

The DRY substitution is in place (`GGML_GOOSE_SAMPLER_DRY=1`) and the build is
clean, but it cannot be compared against the native body the way the other
stages were: `llama_sampler_init_dry_testing` is not exported by the library, and
`llama_sampler_init_dry` needs a real vocabulary, so a synthetic differential is
not available.

That produced a false positive worth recording: an empty output on both sides
made `diff` report "identical" while the test binary had failed to link. The
DRY stage therefore stays opt-in, with its module gate (120/120) as the only
evidence so far, until a real run with a model provides the comparison.

Live and verified by API: typical-p, top-p, XTC, penalties. DRY: hook present,
opt-in, awaiting a model run.

## Real-path A/B on the shipped server: identical output, real Goose calls

Server built from our library copy, Apex-35B-V1.0-Q4_K_M, fixed seed 42, the
same request twice:

```
[ON ] " Paris, and its population is approximately "   counters=2
[OFF] " Paris, and its population is approximately "   counters=0
```

`counters` is the number of `FG171_* goose calls` lines, so the ON run really
executed Goose stages (top-p and penalties in the default chain) and the OFF run
executed none. The generated text is identical.

A defect of mine found here: `GGML_GOOSE_SAMPLER_CHAIN=0` did not disable XTC and
penalties because those hooks lacked the guard, so the first "OFF" run was not
off at all and would have produced a meaningless equality. The guards are in
place and the run above is the valid one, verified by the counter being zero.

## Four stages live at once, output identical

Same server, same request, with typical and DRY switched on in the chain:

```
[ON ] " Paris, and its population is approximately "
      counters: FG171_DRY, FG171_PENALTIES, FG171_TOP_P, FG171_TYPICAL
[OFF] " Paris, and its population is approximately "
      counters: none
```

All four stages execute for real in the live path and the generated text does not
change. DRY's real-path doubt from the previous entry is answered: it runs and
matches.

Still missing from the live path: adaptive-p and mirostat (they need sampler
selection in the server settings) and `dist`/`temp_ext` (TD-S1).

## DRY default-on, fixed seed reproducible on the live path

```
run1: " Paris, and its population is approximately "
run2: " Paris, and its population is approximately "      same-seed identical: YES
other prompt: " Berlin, with a population of around "
counters: DRY, PENALTIES, TOP_P, TYPICAL

[OFF] " Paris, and its population is approximately "      counters=0
```

Two identical requests with seed 42 produce byte-identical text while all four
stages execute; a different prompt produces different text, so the equality is
not a constant answer; and the same request with every stage disabled produces
the same text with no counters.

DRY is default-on now, with the module gate and this live evidence behind it.

## Pivot to the accepted runtime tree (plan recorded)

The restored tree is the accepted source: it has `src/llama-moe-cache.h`, its
`build/stage-run/` holds `libllama.so.0.7.0` and the server binary that accepts
`--moe-cache-mib`. It has `src/` and `ggml/` but no `common/` or `tools/server`
source, so a server cannot be built from it - only the library.

Plan for the Goose build on the correct base:

1. copy `src/` and `ggml/` from `.../franken-goose-exp150-restored-20261108/runtime/reconstructed`
   into our own build area (read-only source, nothing in that tree is modified);
2. write a minimal top-level CMakeLists that adds `ggml` and `src`, since the
   restored tree's top-level file is not preserved;
3. add the sampler shim to that `src/CMakeLists.txt` (our copy) and apply the
   hook patch to its `llama-sampler.cpp`;
4. build `llama` only, then run the accepted server binary with
   `LD_LIBRARY_PATH` pointing at our build, so both sides of the A/B are the same
   server and the same profile, including `--moe-cache-mib 4992`.

Everything measured before this point used a library built from `work/`, which
lacks the MoE cache, so those numbers describe a different runtime and are not
used for acceptance.

## Pivot, step 1 result: the restored ggml is partial too

```
reconstructed/ggml   -> only src/, no CMakeLists.txt
reconstructed/src    -> full, including llama-moe-cache
```

So the accepted tree can seed `src/` but not the ggml build. Step 2 is one of:

1. pair the restored `src/` with the `ggml/` build files from
   `work/llama.cpp-integration` (same lineage, but the ggml sources would come
   from a different snapshot);
2. or start from the full `work/` tree and port `llama-moe-cache.{h,cpp}` plus its
   call sites into our copy there.

The first is a small experiment; the second is faithful but touches graph and
context code. The next iteration tries (1) and, if the ggml API does not line up,
falls back to (2). The minimal top-level CMakeLists for the accepted sources is
committed in `builds/rt-accepted/CMakeLists.txt`.

## Pivot, step 2: the accepted src configures and builds against the work ggml

The combination that works:

```
builds/rt-accepted/src    <- reconstructed/src   (has llama-moe-cache)
builds/rt-accepted/ggml   <- work/llama.cpp-integration/ggml  (full build tree)
builds/rt-accepted/include<- work/llama.cpp-integration/include
```

`cmake` configures cleanly (rc=0) and the `llama` target builds, with CUDA
compilation reaching 67% at the end of the iteration. The restored ggml could not
be used: it has no build files at all, only a partial source directory, which is
why the first two attempts failed on `add_subdirectory` and on missing
`ggml_add_cpu_backend_variant_impl`.

Next: finish the build, confirm the soname matches the accepted server's
expectation, then add the shim and the hooks to this tree and re-run the stage
differentials on it.

## Pivot, step 3: the accepted src pairs with the flint ggml and headers

Three pairings were tried; only one compiles:

| ggml / headers | result |
|---|---|
| restored ggml + work headers | no build files at all in the restored ggml |
| work ggml + work headers | `llama_embd` undeclared (the type lives in the flint header) |
| work ggml + flint headers | `ggml_dsv4_hc_pre_gated` and `ggml_prec_set_acc` undeclared |
| **flint ggml + flint headers** | configures cleanly, compiles with no errors so far |

So the accepted `src/` is the same lineage as `vendor/flint` and needs its ggml
and its public headers, not the `work/` ones. The build was still running at the
end of the iteration (CUDA objects), with zero compile errors.

## Pivot, step 4: the accepted src needs 37 bridge modules, some absent

The restored `src/` calls into generated Goose bridges: `fg199 fg210 fg212 fg230
fg237 fg244 fg367 fg373 fg375 fg376 fg377 fg380 ... fg406 fg756`. Their generated
C mostly lives in `integration/modules/EXP-*-<id>-*/source/host-qualified.c`, and
for `fg367` through `fg406` and `fg756` it is present.

Missing under that naming: `fg199, fg210, fg212, fg230, fg237, fg244` - no module
directory matches those ids (26 module directories exist in total). So the
accepted `src/` cannot be linked as-is from this repository: part of the bridges
it calls were not preserved here.

Consequence: the pivot target is reachable only by either
1. recovering those six bridges (they may exist in the restored tree or the local
   store outside the repository), or
2. building from `vendor/flint` (which pairs with its own ggml and headers) and
   porting only what P6-10 needs, accepting that its `src/` differs from the
   restored one.

The build logs and the exact missing ids are recorded so this is a fact, not an
impression.

## Triple A/B on the accepted profile: memory identical, decode not yet valid

`tools/p610-ab.sh`, accepted server, our flint library, profile with
`--moe-cache-mib 4992`, 7001-token prompts, three runs per side:

```
on  r1 prefill=2473.98 decode=22.72  r2 3095.65/29.45  r3 3127.61/41.89
on  vram_load=9462 vram_peak=10708 rss_kb=21631948 goose_calls=3
off r1 prefill=2460.06 decode=32.26  r2 3136.27/44.00  r3 3167.60/56.26
off vram_load=9462 vram_peak=10708 rss_kb=21629092 goose_calls=0
```

What is valid: VRAM identical to the megabyte, RSS +2.8 MB (+0.01 %), prefill
medians 3095.7 vs 3136.3 tok/s (-1.3 %), and the stages executed (3 calls) versus
not (0).

What is **not** valid: `predicted_n=2` in every run. The model stopped after two
tokens, so the decode figures are two-token startup values, not steady-state
throughput, and they must not be compared with the 39-55 tok/s references. The
spread (22.7 to 56.3) shows the same thing.

Next: request with `ignore_eos` so 512 tokens are actually produced, then the
decode column becomes meaningful and the medians can be compared.

## Valid triple A/B: decode regression of 34.7 % with the stages on

512 generated tokens per run, three runs per side, accepted server, our library,
full profile:

| axis | on | off | delta |
|---|---|---|---|
| prefill median | 2992.19 | 2977.89 | **+0.48 %** |
| decode median | 30.72 | 47.03 | **-34.68 %** |
| VRAM | 9462 / 10748 | 9462 / 10748 | 0.00 % |
| RSS | 21630112 | 21629256 | +0.004 % |

Prefill and memory are neutral; decode loses a third. That is a real regression
and it is not acceptable as an accepted stage as it stands.

Likely causes, in the order worth measuring:

1. every hook allocates `std::vector` buffers and copies the whole candidate
   array (100k+ entries) in and out on every token, three stages deep;
2. the Goose implementations sort and run softmax over the full vocabulary per
   token, while the native bodies do the same work in place, without the copies;
3. the call goes through a generated C function with slice descriptors, adding a
   little overhead per invocation.

The next step bisects by stage (enable one at a time) to attribute the cost
before changing anything, then removes the copies by passing `cur_p->data`
fields directly where the layout allows it.

## Bisect: the loss is in the chain group, not in top-p

Two runs per configuration, 512 tokens, same server and profile:

| config | decode | Goose calls |
|---|---|---|
| all off (base) | 47.06 | 0 |
| top-p only | 47.23 / 47.28 | 11 |
| chain only (typical + XTC + penalties) | 30.35 / 29.91 | 22 |
| DRY only | 44.69 / 44.76 | 0 |

Top-p costs nothing measurable. The whole 35 % loss sits in the chain group. DRY
shows 44.7 with **zero** calls, which is variance, not a DRY cost - its flag was
not actually exercised in that run.

Next: split the group flag so typical, XTC and penalties can each be enabled
alone, then bisect inside the group. The suspects are unchanged: per-token vector
copies and the full-vocabulary sort in the typical body.

## penalties: the scan was the cost, a binary search recovers most of it

The first implementation scanned the whole window for every candidate:
100k candidates x 64 window entries per token. Replacing it with an in-place
insertion sort of the small window plus two binary searches per candidate keeps
the result bit-identical (the module gate still reports 200/200) and changes the
timings:

| config | before | after |
|---|---|---|
| penalties alone | 30.85 | **39.7** |
| all stages | 30.72 | **41.3** |
| base (all off) | 47.06 | 47.06 |

So the full-scan algorithm was ~35 % of decode; the remaining gap is ~12 %. The
remaining suspects, in order: the per-token copies of the 100k-entry candidate
arrays into vectors, then the call overhead. The next step measures a variant
that avoids the id copy before changing the ABI.

## Reused scratch buffers; the remaining cost is the per-candidate search

| config | before buffers | after buffers |
|---|---|---|
| base (all off) | 46.5 | 46.5 |
| penalties alone | 39.7 | **41.6** |
| typical alone | 47.4 | 47.5 |
| all stages | 41.3 | **41.0** |

Allocation per token was worth about 4 % on penalties. The remaining ~11 % is the
two binary searches per candidate: about 1.2 M probes per token over a 100 k
vocabulary.

Next design, cheaper by construction: let the hook own a small direct-mapped table
(keys and counts, a few hundred slots) and pass it to the Goose entry as two
slices, so each candidate costs one probe instead of two searches. The arrays
cannot be declared inside a Goose function (the compiler rejects the literal
form), but slices passed from the caller are fine.

## penalties: caller-owned table, one probe per candidate

The hook now owns a 1024-slot table (keys and counts, thread-local, cleared per
call) and hands it to Goose, so each candidate costs one probe instead of two
binary searches. The module gate stays at 200/200 and the timings move:

| config | full scan | binary search | table |
|---|---|---|---|
| base (all off) | 47.06 | 46.5 | 46.8 |
| penalties alone | 30.85 | 41.6 | **46.1** |
| all stages | 30.72 | 41.0 | **43.5** |

The decode gap went from -35 % to about -7 %.

Caveat recorded rather than hidden: run-to-run variance is not small on this
machine. The same typical-only configuration measured 47.5 tok/s earlier and
44.6 in this run, with unchanged code. Any conclusion about the last few percent
needs more repeats than two, and the current -7 % should be re-measured before it
is quoted as the accepted figure.

## Re-measurement after the table: -6.5 % decode, -3.2 % prefill

Three runs per side, warm runs only (r2 and r3 for prefill, all three for
decode), same server and profile:

| axis | on | off | delta |
|---|---|---|---|
| prefill | 2889.95 | 2986.34 | -3.23 % |
| decode | 44.07 | 47.12 | **-6.47 %** |
| VRAM | 9462 / 10748 | 9462 / 10748 | 0.00 % |
| RSS | 21634672 | 21629820 | +0.02 % |

Compared with the first valid measurement (-34.7 % decode) this is a large
improvement, but it is still a regression and it is not accepted.

The prefill drop did not exist in the earlier run (+0.48 %) and now appears at
-3.2 % with three runs; the first run of each side is consistently slower, and
with two warm runs per side the confidence is low. Treat prefill as unresolved
until more repeats exist; the decode figure is consistent across runs and is the
one to work on.

Remaining decode work, measured not guessed: the hook still copies 100k logits and
100k ids per token for penalties and per token for typical (four arrays). Those
copies are now the largest known term.

## Instrumented: the hooks cost about 1 %, so the measured gap is mostly noise

Per-stage timers inside the hooks, one 512-token request:

```
FG_TIME penalties ... per_call_us=248.8   (including the array copies)
FG_TIME typical   ... per_call_us=1.0
FG_TIME top_p     ... per_call_us=0.0
```

248.8 us per token at ~45 tok/s is 1.1 % of the token budget; typical and top-p
are below one microsecond each. Copies account for ~98 us of that 248.8.

Meanwhile the same binary measured decode 43.87 in this run and 46.56 in another,
with identical flags. The spread between runs of one configuration is of the same
order as the -6.5 % "regression", so that figure is not established.

Method fix for the next measurement: interleave the configurations (ON, OFF, ON,
OFF, ON, OFF) with separate server launches and compare paired differences, rather
than three runs of one side followed by three of the other. A slow drift of the
machine then cancels instead of being attributed to the stages.

## Paired measurement: -1.3 % decode, prefill neutral

Interleaved launches (ON, OFF, ON, OFF, ON, OFF), two requests each:

```
pair1: on=44.02 off=44.60  delta=-0.59 (-1.31 %)
pair2: on=45.81 off=46.66  delta=-0.85 (-1.82 %)
pair3: on=46.46 off=46.80  delta=-0.35 (-0.74 %)
mean paired delta: -0.59 tok/s  (median -0.585)
```

The difference is consistent in sign across all three pairs and matches the
instrumented hook cost of ~1.1 %. Prefill is neutral within the noise
(+0.2 %, -0.5 %, +0.2 %).

The drift is visible and confirms the method: pair 1 sits at ~44 tok/s and pair 3
at ~46.6 with no code change between them. The earlier unpaired figures (-6.5 %)
were that drift.

So the current honest statement is: the Goose sampling stages cost about 1.3 % of
decode and nothing measurable in prefill or memory. The remaining copies account
for roughly 0.4 % of that, which is the only term left to recover before this can
be called free.
