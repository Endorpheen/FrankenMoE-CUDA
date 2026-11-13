# EXP283: CUDA argsort (bitonic) in Goose

`k_argsort_f32_i32` from `ggml-cuda/argsort.cu`: one row per block, shared index
array, the bitonic (k, j) network with a barrier per stage, the padding rule and
the final copy without padding. P3-05's sort item.

## Verdict

**OFF_PATH** — bit-exact against the native bitonic kernel for both sort orders,
but the fixed 32K/128 Apex profile does not execute ARGSORT. No performance claim,
shipped DSO unchanged.

## Scope transferred

Device: shared index initialization, `col ^ j` pairing with the
lower-index-owner rule, the `(col & k)` direction bit, the ASC/DESC strict
comparisons (ties stay in place), the `index >= ncols` padding rule, the swap and
the final copy. Shared memory and barriers are the EXP279 primitives; every
comparison and index is Goose.

Host: `ncols_pad = next_power_of_2(ncols)`, `block = min(ncols_pad, 1024)`,
grid `(nrows,1,1)`, `shared = ncols_pad*4`, and the shared-memory limit check
(native asserts it; the module rejects with code 24).

The CUB path is not built in this tree (`GGML_CUDA_USE_CUB` undefined), so the
bitonic kernel is the native path.

## Defect in my own work, found by the gate

The first build matched row 0 and failed every later row: `bitstep` received the
tensor base `x`, while native passes `x_row = x + row*ncols`. Row 0 has offset 0,
so it hid the mistake. Fixed by passing the row base; the gate then went to zero.

## Gate

| check | result |
|---|---|
| device: 9 shapes (ncols 1,2,3,5,8,16,33,100,1024; nrows 1…4), both orders, heavy ties | 2 910 index values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 8 plans (adds ncols=300 so pad=512) + 3 rejections | 54 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| ASC first-branch compare uses `>=` (ties swap) | 1 368 |
| direction bit uses `j` instead of `k` | 2 822 |
| padding test `a >= ncols` weakened to `a > ncols` | 35 |
| swap writes the second value to the wrong slot | 2 856 |
| host block cap 1024 -> 512 | 1 |

Two lessons. Removing the "lower-index owner" guard (`ixj <= col`) is **not
observable**: both owners then write the same swapped pair, and the swap is
idempotent at that granularity, so the result is unchanged. It was replaced with
the `>=` tie-break mutation, which changes duplicate order. And the host
mutation needed a case with `pad = 512`, the only range where the cap matters;
`ncols=300` was added.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
