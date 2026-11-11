# EXP274: CUDA arange in Goose (device value + host plan)

`arange_f32` and the host contract from `ggml-cuda/arange.cu`: per-index value
`start + step*nidx`, block count `ceil(ne0/256)`, and the element-count check
`steps = (int64_t)ceil((stop-start)/step)` equal to the destination element
count.

## Verdict

**OFF_PATH** — bit-exact against the native kernel, but the fixed 32K/128 Apex
profile does not execute ARANGE. No performance claim, shipped DSO unchanged.

## Scope transferred

- device: the bound check and `start + step*nidx` (all Goose; extern is a float
  store).
- host: the block count, the `steps` computation with a scalar `ceil`, the
  validation against the element count, and rejection of a null destination,
  zero `ne0` and zero `step`.

## Oracle, stated explicitly

The native kernel is defined inside `arange.cu`, not a header. The fixture uses
a **verbatim copy** of its body in `reference/arange-kernel.cuh` with the source
sha256 (`e9bef1d5…c57a5e`) recorded; only the kernel name changed so it can live
next to the Goose kernel. The host reference in the fixture transcribes the two
native formulas (`ceilf` and the 256 block size) and is used only to compare the
Goose host plan.

## Gate

| check | result |
|---|---|
| device: 7 sizes × 4 (start, step) pairs | 419 860 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 5 valid plans + 4 rejection cases | 28 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| device value uses `nidx+1` | 419 860 |
| device value subtracts the step term | 419 832 |
| device bound skips the last element | 28 |
| host `steps` uses truncation instead of `ceil` | 1 |
| host block size 256 -> 128 | 5 |

## Not integrated

Same rule as EXP245–273: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
