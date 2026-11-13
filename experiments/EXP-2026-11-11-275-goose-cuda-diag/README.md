# EXP275: CUDA diag in Goose (F32 and F16)

`diag_kernel` and the host contract from `ggml-cuda/diag.cu`: multi-dim index
decomposition, destination index, the diagonal selection (`i0 == i1`), batch and
source index, the copy-or-zero write, and the host block count with the shape
contract.

## Verdict

**OFF_PATH** — bit-exact against the native kernel for both supported types, but
the fixed 32K/128 Apex profile does not execute DIAG. No performance claim,
shipped DSO unchanged.

## Scope transferred

- device: `i0..i3`, `dst_idx`, the diagonal test, `batch_idx`, `src_idx`, and the
  copy-or-zero write, for F32 and F16. The F16 path uses 16-bit loads/stores so a
  diagonal half value passes through bit-for-bit (NaN payloads included).
- host: `num_blocks = ceil(n_elems/256)`, positive extents, `ne1 == ne0`, and
  `n_elems == ne0*ne1*ne2*ne3`; null pointers rejected.

## Oracle

The kernel lives inside `diag.cu`, not a header. `reference/diag-kernel.cuh`
holds a **verbatim copy** of its body with the source sha256
(`9d683fe8…795299`) recorded; only the kernel name changed.

## Gate

| check | result |
|---|---|
| device F32 + F16, 7 shapes | 132 498 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 4 valid plans + 5 rejection cases | 26 checks / 0 failures |

Shapes include `ne1 != ne0` cases (4×6, 3×5×2×3) because the device kernel is
generic over extents even though the host contract requires `ne1 == ne0`.

## Mutations — and two generator gaps found

| mutation | failures |
|---|---:|
| diagonal test inverted (F32 body) | 66 249 |
| `i1` divisor `ne0` -> `ne1` (F32 body) | 144 |
| `i2` divisor drops `ne1` (F32 body) | 364 |
| host product check weakened to `>=` | 1 |
| host block size 256 -> 128 | 4 |

The first run missed two mutations, both because the fixture did not span them:

1. every shape had `ne1 == ne0`, so `(gidx/ne0)%ne1` and `(gidx/ne1)%ne1` were
   identical. Fixed by adding 4×6 and 3×5×2×3 device shapes.
2. the only product-mismatch case had `ne < product`, which the weakened `>=`
   check still rejects. Fixed by adding a `ne > product` case (17 vs 16).

This is the handoff's "when a mutation passes, suspect the generator" rule in
its usual form: a passing mutation was a fixture hole, not a module property.

## Not integrated

Same rule as EXP245–274: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
