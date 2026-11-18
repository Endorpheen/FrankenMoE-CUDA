# EXP-2026-11-18-1166: P6-11 slice 1, draft-model compatibility

`common_speculative_are_compatible` decides from vocabulary scalars only: vocab type
equality, `add_bos` agreement plus the bos id when the flag is set, the same for eos, and a
bounded vocabulary-size difference (`SPEC_VOCAB_MAX_SIZE_DIFFERENCE`).

`src/compat.goose` transcribes the decision order. The gate is a table of ten cases, each
isolating one condition:

```
spec compat: cases=10 fails=0
```

The first run reported one mismatch, and it was my table rather than the predicate: the
case named "no add_eos" had set the target's add_eos flag to 1 while the draft's was 0, so
the native verdict is correctly "incompatible" and my expected value was wrong. Corrected.

Limits, stated: this gate is a table derived from the native decision order, not a call
into a native instance with two real vocabularies. The live comparison belongs to the MTP
acceptance step, where two models are loaded anyway.

## Slice 2: prefix agreement

`g_spec_prefix` returns the length of the agreeing prefix between the drafted tokens and
the tokens the target produced, and writes the first disagreement position, which is the
rebase point the native `accept()` uses.

```
spec prefix: cases=7 fails=0
```

Seven cases: full agreement, disagreement at each of the three positions, `k=0`, `k=1`, and
a window shorter than the arrays.

Scope note carried from the scope file: in this tree acceptance is target verification, so
this predicate is the whole decision - there is no probability-ratio test to port, and the
predicate consumes no RNG, which keeps the fixed-seed guarantee from P6-10 intact.

## Slice 3: ngram-mod acceptance bookkeeping

`g_ngram_accept` carries the low-acceptance rule of
`common_speculative_impl_ngram_mod::accept`: below 0.25 the streak increments, five in a
row resets the ngram container and the prompt position, a good step clears the streak.

```
ngram accept: steps=14 fails=0 final_low=0
```

The gate applies a 14-step sequence to both implementations and compares the streak and the
reset flag at every step. The sequence deliberately includes the fifth-low reset, a high
step that clears the streak, a zero-length draft that must be a no-op, and **exactly 0.25**,
which is the boundary where a division and the integer form `4 * n_accepted < n_draft_last`
could disagree. They do not.

The other three accept implementations (ngram-cache, EAGLE3, MTP) carry their own state
updates; they are listed in the scope file and are the next two slices, not silently
assumed equal.

## Slice 3b: EAGLE3 / MTP deferred-boundary rebase

`g_eagle_accept` reproduces the guard order of the native `accept`: sequence range first,
then the row count, then `i_g = min(n_accepted, n_rows - 1)` and the rebased position
`verify_pos_first + i_g` that the deferred boundary takes.

```
eagle accept: cases=8 fails=0
```

Eight cases: mid rejection, every draft accepted (the clamp is what makes this correct),
nothing accepted, an accepted count far above the rows, a single-row batch, an empty verify
batch, and both out-of-range sequence directions.

The row copy itself (`pending_g_last = verify_g[i_g]`) is a memory operation on an
`n_embd_dec`-sized slice. It is not ported yet, and it is recorded here rather than implied:
the arithmetic decides *which* row, the copy remains host-side until the embedding buffers
are Goose-owned.
