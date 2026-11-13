# The chain ships the rejected MMQ variant

While preparing to apply the `ne2` fix in-chain I traced which object the running DSO actually links.
It is not EXP180's.

## Verified

- The pinned DSO's link line (recorded in `runtime/exp208/build/commands.json`, label
  `cuda-mmq-q8-1-link`) contains `runtime/exp208/build/cuda-mmq-q8-1.o`.
- That object's build command compiles
  `experiments/EXP-2026-11-09-208-goose-mmq-typed-x/source/cuda-runtime.cu` — the **typed**
  (`x: const f32&`) variant.
- EXP208's own README records the verdict on exactly that variant:

  | variant | SASS | kernel timing |
  |---|---:|---|
  | u64 | 432 | 1848.8 / 1831.9 ns |
  | **typed (in-chain)** | 488 (+56) | 1922.6 / 1958.4 ns |
  | | | **typed is 4-6.5% slower** |

  Both byte-exact (4/4), both sanitizer-clean. EXP208 rejected the typed variant.

So the runtime links the variant its own experiment rejected, while the u64 variant it kept as better
is not in the chain.

## What that implies about the numbers

EXP208's measured **u64** timings, 1848.8 and 1831.9 ns, sit at parity with native as I measured it in
this session (1812.5, 1842.0, 1773.0, 1811.0 ns). EXP180's own numbers (2055, 2103 ns) were measured
before the `ne2` work and on an older emission — 400 vs EXP208's 432 instructions for the same u64
code, which is the extraction-method difference already documented in EXP221.

That resolves an apparent contradiction that had been standing in my notes: EXP180 said the port was
13% slower, EXP208's u64 said parity. They are different emissions of the same source, and the newer
one is at parity.

## The concrete repair, in order

1. Build the chain object from the **u64** source, not the typed one.
2. Include the EXP222 `ne2` fix (164 instructions against native's 157, −3.69% at kernel level,
   byte-exact) — the u64 source is exactly where that fix applies.
3. Measure with the interleaved protocol; EXP224 showed the current in-chain variant is within noise
   (mean +0.37% decode, one pair at −2.62%), so a better variant should clear 2% comfortably.
4. Then flip the switch, which today is `strcmp(flag,"1")==0` — opt-in, against rule 10.

## Not verified here

I have not built or measured the u64+ne2 chain object. Everything above about "should be faster" is an
inference from EXP208's own u64 numbers and EXP222's kernel-level result, not a measurement of the
combined artifact. That measurement is step 3 and it has not been run.
