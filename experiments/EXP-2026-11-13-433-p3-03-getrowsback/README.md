# EXP433: CUDA GET_ROWS_BACK in Goose

`k_get_rows_back_float` is the backward gather: for each output column, sum the
gradient rows whose index equals the destination row. The host plan is block
256, grid `(ceil(ncols/256), min(nrows_dst, 65535), 1)`.

## Gate

The fixture contains a verbatim copy of the native kernel and compares the whole
destination buffer bit for bit.

| case | values |
|---|---:|
| 10 cases: empty gradient, 1x1, tails, 256/257/1000 columns, 1024 gradient rows, 70 000 destination rows (grid-y clamp) | **2 592 719 values, 0 failures**, 0 launch failures |

Sanitizers: memcheck, racecheck, initcheck and synccheck all 0. Mutations 5/5:
row equality inverted, sum becomes difference, destination offset loses
`ncols`, row-index stride 4 -> 8, destination stride `gy -> gy+1`.

## Scope

This is a backward/training utility, not an inference op. It is an OFF_PATH
deliverable under the EXP245/246/247 rule: implemented, gated, not linked into
the shipped inference DSO. The inference runtime does not call it; the training
scope was retired from P4-04.

## Reproduce

    bash experiments/EXP-2026-11-13-433-p3-03-getrowsback/build_fixture.sh
    runtime/exp433/build/gpu-fixture
    python3 experiments/EXP-2026-11-13-433-p3-03-getrowsback/mutate.py
