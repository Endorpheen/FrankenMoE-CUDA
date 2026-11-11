# EXP277: CUDA add_id in Goose

`add_id_kernel` and the host plan from `ggml-cuda/add-id.cu`: per `(i1, i2)`
block, read the row id from `src2`, then `dst_row[i0] = src0_row[i0] +
src1_row[i0]` over the `ne0` columns with the `blockDim` stride.

## Verdict

**OFF_PATH** — bit-exact against the native kernel, but the fixed 32K/128 Apex
profile does not execute ADD_ID. No performance claim, shipped DSO unchanged.

## Scope transferred

- device: id load, the `nb1`/`nb2` destination rows, the `nb01`/`nb02` source
  rows, the `i11*nb11` selected source row, and the strided add loop.
- host: `threads = min(ne0, 768)`, grid `(ne1, ne2)`, positive extents, all four
  pointers present, and the `nb00 == nb10 == nb20 == 4` stride contract.

## Oracle

The kernel lives inside `add-id.cu`, not a header. `reference/add-id-kernel.cuh`
holds a **verbatim copy** of its body with the source sha256
(`f696adbc…f3881b4840`) recorded; only the kernel name changed.

## Gate

| check | result |
|---|---|
| device: 5 shapes (4×2×2, 768×3×1, 1000×2×4, 1×1×1, and a padded-src1 16×3×2) | 10 417 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 4 valid plans + 3 rejections | 26 checks / 0 failures |

## Mutations — and a fixture hole found

| mutation | failures |
|---|---:|
| src1 row uses `nb01` instead of `nb11` | 74 |
| dst row drops the batch term | 7 600 |
| adds `src0` twice | 10 364 |
| loop drops the last column | 22 |
| host thread cap 768 -> 767 | 1 |

The first run missed the `nb11`/`nb01` mutation with **0 failures**: every shape
had `nb11 == nb01`, so the substitution was invisible. Added a padded-src1 shape
(`nb11 = 4*(ne0+2)`), after which the mutation fails 74 values. This is the third
time in this series that a passing mutation was a fixture hole; the rule keeps
paying for itself.

## Not integrated

Same rule as EXP245–276: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
