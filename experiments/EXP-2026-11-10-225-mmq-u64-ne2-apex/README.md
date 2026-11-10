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
