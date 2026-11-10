# EXP223: what the `ensure()` guards cost — count conclusive, timing not

Step 1 of the plan EXP222 recorded: measure the validation. Probe variant, **not correct** — it
accepts inputs the real module rejects (`fn ensure(v,c){if false&&v{fail(c);}}`).

## Instruction counts — deterministic, conclusive

| variant | kernel `<0>` (direct) | kernel `<1>` (scatter) | `BPT.TRAP` |
|---|---:|---:|---:|
| baseline | 400 | 355 | 6 / — |
| `ne2` pinned (EXP222) | 164 | 355 | 0 / 6 |
| `ne2` pinned + guards off | **128** | **206** | 0 |
| native DS4 | 157 | 227 | 0 |

So the guards are worth **36 instructions on the direct path and 149 on the scatter path** — the
scatter path inlines far more `ensure()` sites. With them removed, the emission is *smaller* than
native on both kernels (128 vs 157, 206 vs 227).

Put together with EXP222, the whole instruction gap now accounts for itself:

    direct  400 = 236 (dead ne2 division) + 36 (guards) + 128 (live+address)
    scatter 355 = 149 (guards) + 206

Nothing is left unexplained. There is no mysterious codegen deficiency in the emission — it was a
dead branch plus validation native does not perform.

## Timing — this run is not usable

    goose  2016.1   native 1830.9   +10.12%
    goose  2032.0   native 1955.8   +3.90%
    goose  2224.1   native 1971.8   +12.80%

The **native arm drifts monotonically upward** across the three runs (1830.9 → 1955.8 → 1971.8) while
the comparison it is supposed to anchor is a fixed native kernel that cannot change. Whatever the
cause — thermal, background load — the control moved, so the ratios are not comparable to each other
or to EXP222's clean run (goose 2032/1983/1991 against native 1812/1773/1811).

I am not reporting a timing conclusion from this. The instruction counts above need no timing to be
true; the question "how much time do the guards cost" does, and it is unanswered.

## What this changes for the plan

The recorded next step was "gate `ensure()` behind a compile-time debug flag and measure". The
*structure* answer is now known and is a strong one: removing the guards makes the kernel smaller
than native, so the guards are the last structural difference. Whether they cost measurable time
still needs a clean interleaved run, and they cannot simply be deleted — they are the checks that
caught real fixture bugs in EXP176 and EXP181.

The honest options for a promotable change, in preference order:

1. keep validation but hoist it out of the per-element path where the invariants are loop-invariant;
2. make it debug-only for the Apex build, accepting that a malformed input traps differently;
3. leave it and accept the current gap.

Option 1 is the only one that keeps both properties, and it is source work in the Goose module rather
than a flag.
