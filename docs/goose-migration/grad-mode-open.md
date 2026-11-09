# test-backend-ops `grad` mode: four failures, not yet reproduced in isolation

Status: open, reproducible only in a full run.
Found: 2026-11-08 against `runtime/exp196/run`.

## What happens

    test-backend-ops grad -b CUDA0

    run 1: rc=134, 4 FAIL
    run 2: rc=134, 4 FAIL

    SUM(type=f32,ne_a=[128,2,2,2],v=0): [SUM] nonfinite gradient at index 0 (CUDA0=inf)
    SUM(type=f32,ne_a=[128,2,2,2],v=0): compare failed
    SUM(type=f32,ne_a=[5,7,11,13],v=0): [SUM] nonfinite gradient at index 1 (CUDA0=inf)
    SUM(type=f32,ne_a=[5,7,11,13],v=0): compare failed

The run then aborts after `REGLU`.

## It is not reproduced by any filter tried

| filter | result |
|---|---|
| `-o SUM` | 0 failures |
| `-o ADD,SUM` | 0 failures |
| `-o REGLU,SUM` | 0 failures |
| `-o GEGLU,SUM` | 0 failures |
| `-o SILU,SUM` | 0 failures |
| `-o NORM,SUM` | 0 failures |
| every `GGML_GOOSE_CUDA_*` set to 0, `-o SUM` | 0 failures |

So `SUM` alone passes, and the pairs that precede it in the full run pass. The failure needs a
longer sequence, which is consistent with state carried between cases — a buffer left holding
`inf` from an earlier case, or a gradient accumulated into a reused allocation.

## Not ours: the same four failures occur with every Goose switch off

Ran the full `grad` suite with all 24 `GGML_GOOSE_CUDA_*` switches plus `GGML_GOOSE_RMS` set to
`0`:

    rc=134, 4 FAIL
    SUM(type=f32,ne_a=[128,2,2,2],v=0): nonfinite gradient at index 8 (CUDA0=inf)
    SUM(type=f32,ne_a=[5,7,11,13],v=0): nonfinite gradient at index 3 (CUDA0=inf)

Same operator, same count, same `inf` signature. The failing index differs between runs, which
confirms the non-determinism seen before. So this is **pre-existing in the baseline**, not a
product of the Goose port, and it does not block P3-19.

That is the arm the earlier note was missing, and it reverses the "state carried between cases,
possibly ours" reading: it is state carried between cases in the native build too.

## Why this is recorded rather than worked around

`grad` is the backpropagation-comparison mode; the Apex inference path does not use gradients,
so this does not affect the deployed profile. It is recorded because P3-19 is about the CUDA
backend as a whole, and because a failure that only appears in sequence is exactly the kind that
gets explained away.

The next step is a bisection over the operator list: run the full set, then halves, until the
minimal failing sequence is found. That needs the harness to accept an ordered list, which it
does through `-o` with several names, so it is mechanical.

## Related: EXP186's sanitizer errors are also native

Re-ran memcheck on the rebuilt fixtures after the `sample_ratio` fixes. EXP184, EXP185 and EXP188
are clean. EXP186 reports 10 errors, and they are the same ones recorded earlier:

    Invalid __global__ read of size 2 bytes
    at void mul_mat_vec_q<(ggml_type)14, (int)1, (bool)0, (bool)1, (bool)0>(...)

`(bool)1` is the template parameter, i.e. the **native** kernel the fixture runs as its reference
arm. Searching the whole log for the Goose symbol returns zero: no error is attributed to
`fg186*`, and the Goose arm alone reports none.

This mattered to check because the `sample_ratio` change touched EXP186's body, and an
out-of-bounds read in the Goose arm would have been a defect introduced by that fix. It is not:
the read is in the native template at block 4, which is the same finding already recorded rather
than a new one.

Worth stating plainly, since the raw count "10 errors" will otherwise look alarming in the
sanitizer sweep: it is a property of the reference arm of an opt-in experiment that is not in the
active link.
