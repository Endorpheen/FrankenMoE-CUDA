# EXP207: per-sequence position index — BLOCKED, not promoted

`seq_pos_min`/`seq_pos_max` were replaced by a Goose scan over the cells carrying a sequence.

## What works

    FG207P_KV_SEQPOS calls=99874
    FG207P_FIXTURE checks=99874 failures=0

The gate is real and it is load-bearing: reverting the empty-cell filter makes it fail with 2130
errors. That filter is the defect I found here -- native `seq_pos` never holds an empty cell
(`rm()` resets the sequence bits before the position becomes -1), and the first scan included
them, so `min` came back as -1.

## What is blocked, and the exact evidence

Live wiring fails with **HTTP 500, "Context size has been exceeded"**, and `FG207P calls=34`.

The decisive control: the same build with **`GGML_GOOSE_KV_SEQPOS=0` fails identically**. So the
Goose scan is not the cause. The cause is in the wiring, and the EXP206 ODR trap is the suspect:
the referenced copy of `llama-kv-cells.h` had grown two `mutable std::vector` members for staging,
which changes `sizeof(llama_kv_cells)`. Every translation unit built against the unmodified header
then disagrees on the layout of the object it is manipulating. EXP206 hit exactly this and
recorded it; I repeated it.

The staging members were moved out of the class into accessors that fill caller-owned vectors, so
`sizeof` is unchanged, but the rerun after that change is not recorded here -- the verdict is
therefore **BLOCKED, not accepted**, and the module is not in the active chain.

EXP206 remains the last accepted checkpoint (2554.17 / 26.08 on a recheck).

## What this does not say

It does not say the scan is too slow, and it does not say the algorithm is wrong. The gate covers
the algorithm; the control isolates the failure to the wiring. A future attempt should verify the
control passes with the switch off *before* looking at the Goose numbers, which is the check I ran
too late.
