# `test-backend-ops perf` on the active runtime

Ran the perf mode as an independent check, separate from the correctness matrix and from Apex.

## Result

It completes and passes, in both arms, for every module tried:

    RMS_NORM   Goose OK, 2/2 backends
    GET_ROWS   Goose OK, native arm OK
    CPY        Goose OK, native arm OK
    CONCAT     Goose OK, native arm OK

No crashes, no aborts, and the CUDA VMM/pinned witnesses read `native=0` on the Goose arm and the
expected counters on the native arm, so the switches take effect under this mode too.

## But it prints no timings in this build

The output is the pass/fail summary only — no per-case GFLOPS, GB/s or microseconds. So this run
establishes that the perf path executes without error, **not** that any module is faster or
slower. It is recorded that way deliberately: it would be easy to report "perf passed" as if it
were a performance result, and it is not one.

Performance claims in these documents continue to rest on paired Apex runs, which measure the
whole model with one variable switched, and those are the only figures with a stated protocol.

The full-matrix `perf` run was abandoned after ten minutes — it enumerates every case of every
operator and is not a useful unit of work here. The per-operator form above is the practical one.

## What would make this useful

If timings were printed, this mode would be a good third instrument: it isolates one operator at
a time on the device, without the model, and would catch a per-kernel regression that a whole-model
Apex pair could absorb into noise. Worth revisiting if the harness output is ever enabled.
