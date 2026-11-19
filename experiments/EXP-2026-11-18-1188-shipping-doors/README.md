# EXP-2026-11-18-1188: the accepted stages re-measured through the shipping doors

The chain now addresses its entry points by exported name (EXP-1187). That changed the ABI, so
the accepted numbers were re-measured against the library that is actually linked into
`llama-server`, not against an experiment build.

Method: the existing differential harness is forwarded to the shipping doors by a small C file,
one door per stage, and linked against `builds/flint-acc/bin/libllama.so`.

```
typical-p:      cases=1800 mismatches=0
top-p unsorted: cases=900  mismatches=0
top-p large:    cases=45   mismatches=0
xtc:            cases=720  mismatches=0
```

So typical-p, top-p and XTC are bit-exact through the exported doors, and the accepted results
carry over to the new calling mechanism.

## Two harness defects found on the way, both mine

1. **Stale op numbers.** The harness called the dispatcher with `op=2` for XTC, which is top-k
   in the current module, and my first forwarder compounded it by passing arguments in the wrong
   order. One door per stage, addressed by name, removes the possibility: there is no op number
   left to be stale.
2. **An aliased scratch buffer.** The typical door writes into its `scr` parameter; my first
   forwarder passed the logits array as that scratch, so the comparison ran against mutated
   input. A separate buffer fixed it.

## DRY, measured after closing the gap

The gap below was closed the same day by exporting the decision the harness actually needs:

```
export fn fg_dry_rep_limit_entry(last, b_head, b_ts, b_tl, b_tail, n, nb) -> i64
```

and pointing `gate_dry_c` at it. The full differential is now green against the shipping build:

```
typical-p:      cases=1800 mismatches=0
top-p unsorted: cases=900  mismatches=0
top-p large:    cases=45   mismatches=0
xtc:            cases=720  mismatches=0
dry step1:      cases=200  mismatches=0
```

## The gap as it stood, and why it mattered

The harness's DRY case calls `gate_dry_c`, which returns the repetition limit position. That
forwarder no longer exists anywhere in the tree - the earlier experiment that provided it was
not kept as source - so the call was re-pointed at `fg_sampler_dry_call`, which implements a
different contract, and 153 of 200 cases disagree because of that, not because of the module.
Recorded as a gap: the DRY differential needs its forwarder written again against the exported
door before its numbers can be claimed for the new build.

## Adaptive-P and Mirostat, the two stages still listed as open

```
adaptive-p: cases=33211 mismatches=0
mirostat-k: cases=500   mismatches=0
```

Adaptive-P compares every transformed logit against the native formula
`PEAK - SHARPNESS * d^2 / (1 + d)` with `d = |p - adapted| / 0.3` and
`adapted = clamp(2*target - weighted_sum/total_weight)`, over 1000 random vocabularies
(33 211 individual logits), including the degenerate `total_weight == 0` branch.

## My reference was wrong again, and it was the interesting case

The first Mirostat run reported two mismatches out of 500, both where `k_f` sat just above an
integer:

```
MISMATCH n=10 mu=2.70251 m=5 got=2 want=3 (kf=2.06183)
```

The native code ends with `llama_sampler_top_k_impl(cur_p, std::max(int(k), 1))` - it
**truncates** the float, it does not round up. My reference used a ceiling, so the port was
right and the test was wrong. With `(int64_t) kf` the two agree on all 500 cases.

That is worth keeping in the record: a ceiling looks like the careful choice and is the wrong
one here; only reading the call site settles it.

## The RNG stream, through the shipping door

```
rng stream: cases=1280 mismatches=0
```

Four seeds, 320 draws each, comparing the module's `dist` door against `std::mt19937` advanced in
parallel. Because the door consumes **two words per draw**, an identical sequence also proves the
stream position advances identically - a one-word offset would show up immediately.

### Two things my first attempt got wrong, both in the harness

The first run reported 970 mismatches out of 1280, and both causes were mine:

1. **Wrong seeding.** I filled the state with 624 successive `mt()` calls. `std::mt19937(seed)`
   uses the recurrence `1812433253 * (prev ^ (prev >> 30)) + i`, and the module implements
   exactly that in `sm_mt_seed`. Two different expansions of the same seed give two different
   streams, so the comparison was between unrelated generators.
2. **Wrong canonical double.** I used `std::generate_canonical<double, 53>`, which scales by the
   mantissa. This project's draw is `(low + high * 2^32) / 2^64` - low word first - as in
   `mt_canonical` in the module. Both are plausible; only one is the one the sampler uses.

Both are the same failure mode as the field-name mistake earlier in this card: a harness written
from what the standard *usually* does, instead of from what this code does.
