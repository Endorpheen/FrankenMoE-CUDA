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

## Slice 3c: ngram value selection

`g_ngram_pick` carries the complex-mode decision of `common_ngram_map_send`: scan the four
slots for the maximum with a **strict** comparison (so a tie keeps the first slot), sum the
others, refuse unless the maximum is at least twice that sum, and clamp the draft length to
`min(m, n_accepted[slot])`.

```
ngram pick: cases=8 fails=0
```

Eight cases, and two of them are the ones that matter:

- **exactly 2x**: `4` against `1+1` is accepted, which proves the comparison stayed strict
  (`max < 2 * sum` refuses, `max == 2 * sum` does not);
- **tie keeps first**: `5,5` with a big second slot is refused, which proves the scan uses
  `>` rather than `>=` and did not pick the second slot.

The remaining piece of this path, `common_ngram_map_accept` (store `n_accepted` into the
chosen slot behind the `last_draft_created` guard), is a two-line store and is left with the
container itself, which is P9-05's subject.

## Slice 4: maximum draft width across configured types

`g_spec_n_max` reproduces `common_speculative_n_max`: the model-based types contribute
`max(0, draft.n_max)`, the three ngram-map types contribute their `size_m`, `NGRAM_MOD`
contributes its own `n_max` clamped at zero, `NGRAM_CACHE` contributes a fixed 8, and
`NONE`/`COUNT` contribute nothing.

```
spec n_max: cases=10 fails=0
```

Ten cases, including a negative draft width (clamped to zero, not treated as a maximum), a
mix where the largest value comes from the third type, and an empty type list. The type ids
are written into the source as a comment against `enum common_speculative_type`, because
guessing them is the mistake this project has already paid for twice.

## Slice 5: type names, compared against the enum rather than re-typed

`g_spec_type_id` maps the eleven names to their ids by byte comparison over `const u8[:]`,
so it needs no string runtime - which matters, because Goose string support is exactly what
rule 27 says must live in Goose data rather than in a C++ container.

```
spec type name: fails=0
```

The gate is stronger than the previous slices: it does not repeat a hand-written list, it
walks `enum common_speculative_type` and asks Goose to map the name it produces for every
member. On top of that, seven near misses must be rejected - empty, wrong case, a truncated
name, a name with a trailing dash, a prefix of a longer name, and two arbitrary strings.

That near-miss set is deliberate: a byte comparison with a wrong length check passes the
positive cases and fails these.

## Slice 6: one level of the ngram cache

`g_try_draft` ports the level decision of `common_ngram_cache::try_draft`, and it has a trap
worth naming: the **best candidate is chosen by the product** `count * static_weight`
(where the weight is `100 * static_count`, or 1 when the token is absent from the static
part), while the **acceptance tests use the plain count**. Writing both with the same number
is a silent behaviour change that no crash would reveal.

```
ngram cache level: cases=7 fails=0
```

The gate compares against the native body transcribed verbatim and against my expected
values, so a wrong transcription fails too. Three cases exist only to catch the product
trap:

- **static weight flips**: a tie in plain counts is decided by the static count;
- **static zero weight**: a token with static count 0 still gets weight 0, not 1, which is
  the difference between "absent" and "present with zero";
- **min percent boundary**: exactly 50 % passes, so the comparison stayed strict.

## Slice 7: static path and its threshold tables

Two functions: `g_static_lax_thresholds` holds the lax tables `{2,2,1,1}` and `{66,50,50,50}`
as a decision rather than as a copied literal, and `g_static_draft` ports the static-only
attempt, which uses a **plain maximum** here (no static weighting, because everything in this
path is static) and indexes the lax table at `LLAMA_NGRAM_STATIC-1`.

```
static ngram: cases=10 fails=0
```

Four threshold cases walk all levels and compare with the constants in `ngram-cache.cpp`; six
draft cases cover the plain maximum, both threshold directions, the sample-size rejection,
the first-wins tie, and a single strong candidate. `LLAMA_NGRAM_STATIC = 2` and
`LLAMA_NGRAM_MAX = 4` are recorded in the source comment against `ngram-cache.h`.

Note the contrast with slice 6, which is exactly why both exist as separate gates: the
static path uses a plain maximum, the primary path uses the product. A single shared
"pick the maximum" helper would have been wrong for one of them.
