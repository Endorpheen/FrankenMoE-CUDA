# EXP278: CUDA acc in Goose

`acc_f32` and the host contract from `ggml-cuda/acc.cu`: `src1_idx = i - offset`,
the stride decomposition `(i13,i12,i11,i10)`, the bounds-guarded accumulate
`dst[i] = x[i] + y[...]`, and the block count.

## Verdict

**OFF_PATH** — bit-exact against the native kernel, but the fixed 32K/128 Apex
profile does not execute ACC. No performance claim, shipped DSO unchanged.

## Scope transferred

- device: the flat bound, `src1_idx`, the four-index decomposition, the
  `i10/i11/i12/i13` bound chain and the accumulate. Divisions are evaluated only
  for `src1_idx >= 0`, where C truncation and Goose's floor agree; negative
  indices take the "copy only" path, which is what the native short-circuit does.
- host: `num_blocks = ceil(ne/256)`, block 256, pointer/extent checks and a
  positive-stride guard. The guard is a deliberate divergence: the native kernel
  divides by `s11/s12/s13` without checking, so zero is undefined behavior there;
  the module rejects it instead of reproducing UB.

## Oracle

The kernel lives inside `acc.cu`, not a header. `reference/acc-kernel.cuh` holds
a **verbatim copy** of its body with the source sha256
(`9463f70b…8ffd3d`) recorded; only the kernel name changed.

## Gate

| check | result |
|---|---|
| device: 6 cases (contiguous offset 0/+5/-3/past-end, gapped strides, 1-element) | 4 301 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 3 valid plans + 3 rejections (null, ne=0, zero stride) | 18 checks / 0 failures |

The gapped-stride case uses `s11 = 5 > ne10 = 3`, so the `i10 < ne10` branch is
exercised rather than always true.

## Mutations

| mutation | failures |
|---|---:|
| `src1_idx` uses `i+offset` | 75 |
| remainder subtracts `i13*s12` | 56 |
| source index drops `i10` | 90 |
| excludes `src1_idx == 0` | 2 |
| host block size 256 -> 128 | 3 |

No fixture hole this time; the size/offset case set covered every branch the
mutations touch.

## Not integrated

Same rule as EXP245–277: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
