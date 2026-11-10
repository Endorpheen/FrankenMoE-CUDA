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

---

# Re-measurement: removing the guards does not change the time

Ran the four-run pattern (native → goose → goose → native) on the no-guard variant to control drift:

    native 1843.2
    goose  2014.7
    goose  2002.4
    native 2069.0

The trailing native reading is 12% above the leading one for a kernel that cannot change, so the
machine heats over successive runs and the last slot is unreliable. The goose readings, however, are
tight and consistent with everything before:

| variant | instructions `<0>` | goose ns observed |
|---|---:|---|
| baseline | 400 | 2054.7, 2102.9 |
| `ne2` pinned | 164 | 2032.2, 1982.8, 1991.1 |
| `ne2` pinned + guards off | 128 | 2016.1, 2032.0, 2014.7, 2002.4 (one 2224.1 outlier) |

Cutting from 164 to 128 instructions — and on the scatter kernel from 355 to 206 — produced **no
measurable change in time**. The gains stop after the first step.

## So instruction count does not predict time here, and that is now measured

Two data points, same harness, same method:

- 400 → 164 (−59% instructions) bought −3.69%;
- 164 → 128 (−22%) plus 355 → 206 (−42% on the other kernel) bought nothing measurable.

If time tracked static code size, the second step would have moved the needle too. It did not. So the
first step's gain most likely came from removing a **branch** (`if ne2i == 1` and its divergence) on
the per-element path rather than from 236 instructions of dead code being absent from the I-cache.

I am careful here: that is the most consistent reading of two measurements, not a proven mechanism.
What is proven is narrower and worth stating plainly — **on this kernel, static instruction count is
not a usable predictor of runtime**, and every hypothesis in the earlier rounds that leaned on it was
leaning on the wrong signal.

## Consequence for the plan

The remaining ~11% gap cannot be closed by making the emission smaller. The two structural items the
investigation produced (dead `ne2` branch, guard volume) are now both quantified and neither explains
the rest. Closing it needs a counter-based answer, which this machine cannot give
(`profiling-unavailable-2026-11-10.md`), or a controlled experiment that isolates a single runtime
property — occupancy, load latency, or the per-element branch — rather than a code-size change.

That is where this line of work stops being productive without profiling.
