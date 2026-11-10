# EXP225: u64 + ne2 in-chain at Apex — inside 2%, but the variant question is not resolvable here

Combines the two findings from EXP208 and EXP222: the chain was linking EXP208's **typed** MMQ variant,
which EXP208 itself rejected as 4-6.5% slower than its u64 source, and EXP222 pinned `ne2` on that u64
source for 400 → 164 SASS instructions against native's 157.

## Build

`runtime/exp225/run` is a copy of `exp220/run` with `libggml-cuda-u64ne2.so.0.27.0` swapped in. That DSO
was linked from `runtime/exp208/build/commands.json`'s own `cuda-mmq-q8-1-link` line with exactly one
substitution: `runtime/exp208/build/cuda-mmq-q8-1.o` → an object compiled from EXP180's
`cuda-runtime.cu` beside EXP222's ne2-pinned device header. `host.o` stays EXP208's, so the dispatcher
and its switch are unchanged. Both arms therefore run the **same DSO**, differing only in the env
variable — the cleanest possible pairing.

## Result

| pair | prefill | decode | tokens | probabilities |
|---|---:|---:|---|---|
| 1 | −1.79% | −4.56% | equal | equal |
| 2 | +0.27% | **+2.12%** | equal | equal |
| **mean** | **−0.76%** | **−1.23%** | equal | equal |

Raw decode: native `[26.72, 26.45]`, u64+ne2 `[25.50, 27.01]`.

## What this establishes

- Correctness: 128 token ids, text and all 128 saved probabilities equal in all four runs.
- The mean is **inside the 2% rule** in both directions.

## What it does not establish, and this is the important part

The two pairs swing **6.7 points** in decode. The u64+ne2 mean (−1.23%) and the typed variant's mean
from EXP224 (+0.37%) differ by 1.6 points — four times smaller than the spread that produced them. **So
this measurement cannot say which variant is better.** Anyone reading "−1.23% versus +0.37%" as a
ranking would be reading noise as signal.

What can be said: both variants are consistent with parity against native at Apex level, the ne2 fix's
59% instruction cut bought no measurable time at this level, and the question "which MMQ variant should
the chain link" needs either many more pairs or a counter-based answer this machine cannot give.

## Consequence

Do not change the chain's linked variant on the strength of this. The honest ordering is:

1. the variant choice should be decided on the kernel-level evidence EXP208 already has (u64 1848.8 /
   1831.9 ns versus typed 1922.6 / 1958.4 ns, byte-exact both) — that comparison was made on one
   harness with tight spread and is more trustworthy than an Apex pair whose arms disagree;
2. the switch direction (currently opt-in, against rule 10) should be decided separately from the
   variant question, so the two changes are not conflated in one measurement;
3. nothing here justifies promoting on performance grounds.

`runtime/exp225/run` is kept because the DSO is reproducible from the recipes above, but it is **not**
a promoted runtime and `active` stays at exp220.

---

# My own protocol was order-biased — these numbers cannot decide a promotion

Extending to five pairs exposed a defect in how I ran them, not in the component.

    pair 1  decode -4.56%
    pair 2  decode +2.12%
    pair 3  decode +3.26%
    pair 4  decode +4.53%
    mean +1.34%   median +2.69%   spread 9.09

The Goose arm rises monotonically across the series (25.50 → 27.01 → 26.78 → 27.43) while native stays
flat (26.72, 26.45, 25.94, 26.24). That is not a property of a kernel. It is the run-order artifact this
repository already documented in `census-status.md`: *"whichever arm runs first pays a warm-up of
roughly 1.3 tok/s."*

Every script I wrote for EXP224 and EXP225 iterates `for arm in A B` inside each round, so **arm A always
runs first** and arm B always runs second. The bias therefore always favours the Goose arm. Corrected:

- EXP224's mean +0.37% is biased **upward** — the true value is more negative;
- EXP225's mean +1.34% is biased **upward** — the true value is more negative;
- the −4.56% first pair is the only one measured without a previous run's warmth behind it, which is
  why it is the outlier on the negative side.

So the honest status of the numbers in this file is: **they are not usable to decide whether to promote
MMQ**, and the direction of the error is known.

## The protocol was documented and I did not follow it

`census-status.md` says all stages are measured "in both orders", and the user restated it as
"старая → новая → новая → старая". Rounds 1 and 2 of EXP224/225 used A,B,A,B — which is four runs but
only one order. The correct schedule alternates: A,B then B,A, so each arm occupies the first slot once.

## What is still valid from these two experiments

- Correctness: every run, both arms, had 128 token ids, text and all 128 saved probabilities equal.
- The build recipe: `runtime/exp225/run` is reproducible from EXP208's recorded link line with the one
  documented object substitution, and both arms genuinely used the same DSO.
- The finding that the chain links the typed variant EXP208 rejected, and that the switch is opt-in
  against rule 10 — neither depends on these timings.

What is **not** valid is any ranking of the two MMQ variants, and any claim that either is inside 2%.

## Next

Re-run with the alternating schedule before any promotion decision. Until then the ledger entry that
says "both means inside 2%" should be read as "both means measured with an upward bias of unknown
size".
