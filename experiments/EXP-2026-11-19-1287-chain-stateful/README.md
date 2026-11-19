# EXP-2026-11-19-1287: DRY holds in composition, typical+XTC does not

```
chain with DRY: identical (DRY counters: 1)
chain stateful: OPEN, 83 rounds differ
```

Two chain configurations, each run with every door enabled and every door disabled, comparing the token selected
each round and the surviving array size.

## Configuration 0 - DRY inside a chain: closed

penalties, DRY, top-k, top-p, temp, dist, with a real accept history that injects breakers (`10`) every fifth
round so DRY has something to read. 100 rounds, identical. This is the composition evidence for the DRY defect
fixed in EXP-2026-11-19-1281: the fix holds when the sampler is fed by the chain rather than by a harness.

## Configuration 1 - typical + XTC + mirostat + adaptive: OPEN

83 of 100 rounds differ. The state the two paths leave after the first two stages is the explanation:

```
goose:  n=51 size=10 sorted_flag=0  [0] id=7 logit=5.181  [3] id=145 logit=5.535
native: n=51 size=7  sorted_flag=1  [0] id=7 logit=5.181  [3] id=77  logit=4.299
```

Two differences, both in the array rather than in a value:

- **the count**: the XTC door keeps 10 where the native path keeps 7, so everything after it - including
  mirostat's truncation width `k` - is computed on a different set;
- **the order**: the goose array is **not monotonic** (entry 3 has a higher logit than entry 0) and is flagged
  unsorted, while the native's stays sorted. The native trims by advancing the base pointer, which preserves
  order; the door compacts in place, which does not.

In isolation both XTC paths keep a sorted array and truncate the same number of times (200 cases, checked
separately). The difference only appears when the array reaching XTC was already modified by `typical` - which is
precisely the composition this card exists to test.

## The fix this produced, and what is left

The count difference had a single cause: **the native XTC sorts before it looks at probability ranks, and the door
branch ran before that softmax**, so after `typical` - which leaves the array unsorted, identically in both paths -
the door ranked an unsorted array. One added call, `llama_sampler_softmax_impl(cur_p, true)` before the branch,
makes the door receive what the native code computes on. With it, the state after `typical`+`XTC` is **byte
identical** in both modes: size 7, sorted, same ids, same logits, same probabilities.

The chain difference fell from 83 rounds to **42**, so something downstream still diverges.

### Isolation, run after the fix

```
typical+xtc+mirostat+dist           34 rounds differ
typical+xtc+adaptive+dist           21 rounds differ
typical+xtc+mirostat+adaptive+dist  56 rounds differ
```

Each sampler alone matches (EXP-2026-11-19-1284), and the state **entering** them is byte-identical after the fix,
so the divergence is in what the door does with that state. A gated trace (`FG_SAMPLER_DIAG=1`) now prints
mirostat's `k` and `mu` per call in both modes, and it found one concrete deviation straight away:

```
goose:  FG_MIROSTAT k=1000000 mu=13.364114761 n_in=2
native: FG_MIROSTAT k=1572443 mu=13.364114761 n_in=2
```

The port stops counting `k` at **1 000 000**; the native has no bound. With `n_in=2` it cannot change the
truncation, so it is a deviation to remove rather than the cause, and the first *round* difference is later (31)
than the first `k` difference (15). It is not the pair any
more, and mirostat and adaptive-p each match in isolation, so the remaining candidate is their **stateful**
interaction: mirostat updates `mu` from the surprise of the token it selected, and a different `k` in one round
changes every later round. The next experiment is to log `k` and `mu` per round in both modes and find the first
round where they part - not to guess which sampler to blame.

The acceptance hash is unchanged (`c2ff42cc1219`), because the fixed profile enables neither typical nor XTC.

### The decisive narrowing: fresh chain versus persistent chain

Running the same stages with a **fresh chain built per round** - same inputs, same seeds - gives **zero differing
lines**:

```
typical+xtc+adaptive+dist, fresh chain each round: differing lines = 0
typical+xtc+adaptive+dist, one chain for 100 rounds: 21 rounds differ
```

So no single arithmetic step differs. What differs is **state carried between rounds**: the EMA in adaptive-p
(`weighted_sum`, `total_weight`), mirostat's `mu`, and the RNG streams of the samplers that hold one. One round
whose selection differs anywhere sets a different `original_probs` entry into the EMA and every later round
follows.

That makes the next measurement specific rather than exploratory: log, per round and in both modes, the selected
token, the sampled index, the EMA state and the number of RNG draws consumed. The first round where any of them
parts is the answer, and it is a single line to add to the `FG_SAMPLER_DIAG` trace that already exists.

### The first divergence is one bit in one probability

The trace prints the array just before the selection, and the first line that differs is this:

```
goose:  FG_ADAPTIVE_IN [1] id=98 logit=409ff2ba p=3efed6c8
native: FG_ADAPTIVE_IN [1] id=98 logit=409ff2ba p=3efed6c9
```

Same token, **identical logit**, and the probability differs in the **last bit**. Everything after it follows: the
discrete distribution is moved by one ULP, the drawn index changes, the EMA updates from that index, and the
cascade reaches every later round.

So the samplers are not wrong in a categorical way - the transform and the logits agree bit for bit - and the
remaining defect is a single rounding difference in one division. That is a much smaller claim than "mirostat and
adaptive-p diverge", and it is the one supported by the data.

**Next measurement, exactly:** dump `e = expf(logit - max)` and `cum` for that array in both modes. If `cum` matches
and `e` matches, the difference is in the final division; if `e` differs, the input to `expf` differs by a bit even
though the printed logit agrees. One line each, in the trace that already exists.

### Original note on the open question

Whether the door should reproduce the native's trimming (advance the base, preserving order) or the caller should
re-sort after the door returns. Both are defensible; choosing between them needs a statement about what a sampler
may do to the array it is given, which is the same open question recorded for `temp_ext` before it turned out to be
my unsorted input. It is **not** on the acceptance path - the fixed profile sets no typical or XTC parameters - so
it is dormant there, and recorded rather than deferred.
