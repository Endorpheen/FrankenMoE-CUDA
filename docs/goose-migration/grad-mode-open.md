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

The all-switches-off arm was run only with `-o SUM`, so it does **not** yet establish whether the
defect is ours; that arm must be repeated on the full run before any attribution is claimed.

## Why this is recorded rather than worked around

`grad` is the backpropagation-comparison mode; the Apex inference path does not use gradients,
so this does not affect the deployed profile. It is recorded because P3-19 is about the CUDA
backend as a whole, and because a failure that only appears in sequence is exactly the kind that
gets explained away.

The next step is a bisection over the operator list: run the full set, then halves, until the
minimal failing sequence is found. That needs the harness to accept an ordered list, which it
does through `-o` with several names, so it is mechanical.
