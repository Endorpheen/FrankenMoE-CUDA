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

### The cause: the two paths label the same array differently

The arrays entering adaptive-p are **monotonic in both modes**, but the flag is not:

```
goose:  FG_ADAPTIVE_FLAG sorted=0 monotonic=1 n=7
native: FG_ADAPTIVE_FLAG sorted=1 monotonic=1 n=7
```

`llama_sampler_softmax_impl` uses `data[0].logit` as the max when the flag is set and scans otherwise. On this array
both give the same answer, so the flag alone is harmless here - but on one round the flag was set on an array whose
first entry was **not** the largest, and then the two paths took a different max. That is exactly the 1-ULP
difference, and it is reproducible outside the sampler:

```
with max=data[0]: p0=3b855bc1 p1=3efed6c9 p2=3eff13c9      <- the native value
with true max   : p0=3b855bc1 p1=3efed6c8 p2=3eff13c9      <- the goose value
```

Three inputs, two max choices, and the observed numbers match one each. The divergence is therefore not in the
transform, not in `expf`, not in the normalisation: it is in **which max each path trusts**, which comes from a flag
the two paths maintain differently.

### The instrument is now suspect, and here is why

The two prints that were supposed to bracket the transform **contradict each other** in both modes. The failing
round reports

```
FG_ADAPTIVE_FLAG sorted=0 monotonic=1 n=3
FG_ADAPTIVE_IN [0] id=61  logit=3e4482c0
FG_ADAPTIVE_IN [1] id=98  logit=409ff2ba
FG_ADAPTIVE_IN [2] id=16  logit=409ffa62
```

`monotonic=1` is computed at **entry**, over the same `cur_p->data`, and an ascending triple cannot be descending.
So either the array is reordered between the two prints - by something I have not accounted for - or one of the two
prints is lying. Neither possibility permits a conclusion about the port.

Counts across the whole run, all 100 rounds:

```
                    sorted=1,mono=0   sorted=1,mono=1   sorted=0,mono=1   sorted=0,mono=0
goose                      0                0                81                19
native                     0               80                 1                19
```

Both modes agree that nothing is mislabelled when it is unsorted, and disagree only on the label of **sorted**
arrays - which cannot change a maximum. The earlier numeric reproduction (`max=data[0]` giving the native value)
was fitted to an array order that this trace now says should not exist at that point.

**So the next step is not a fix.** It is to print the full array at entry as well, so the two prints either agree
or the reordering is located between them.

### What that means, and what is not done

The doors must leave `cur_p->sorted` in the state the native code would: false after a compaction that reorders
(typical, XTC), true after a sort (top-p, the pre-ranking sort added for XTC). Aligning that is a small, checkable
change, but it touches the shared array contract for every door, so it is prepared as its own step and verified
against all five door comparisons plus the two chain configurations - not slipped in at the end of a numerical
investigation.

### Original note on the open question

Whether the door should reproduce the native's trimming (advance the base, preserving order) or the caller should
re-sort after the door returns. Both are defensible; choosing between them needs a statement about what a sampler
may do to the array it is given, which is the same open question recorded for `temp_ext` before it turned out to be
my unsorted input. It is **not** on the acceptance path - the fixed profile sets no typical or XTC parameters - so
it is dormant there, and recorded rather than deferred.
