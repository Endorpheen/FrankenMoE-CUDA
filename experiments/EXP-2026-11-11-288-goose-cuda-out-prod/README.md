# EXP288: CUDA out-prod policy and pointer kernel in Goose

`out-prod.cu` — the last owned piece of P3-10's dense list. The file is 125
lines: one real kernel (`k_compute_out_prod_ptrs`, 20 lines) and a host
function whose entire body is branch selection plus three `cublasSgemm*` calls.

## Verdict

**OFF_PATH** — both gates are exact, but the fixed 32K/128 Apex profile never
issues OUT_PROD, so nothing is linked into the shipped DSO and no performance
or memory claim is made.

## Scope transferred

Device: `k_compute_out_prod_ptrs` in Goose — the thread indices, the bounds
return, the flat index and the three pointer expressions. Native pointer
arithmetic is done on a `float*`, so the byte address is `base + 4*expr`; that
scale is explicit in the port.

Host: the whole decision. Which of the three branches runs
(`dps2 == 1 && ne2 > 1` → strided batched, then `ne2 > 1 || ne3 > 1` → batched
pointers, else single GEMM), the GEMM parameters each branch hands over
(`lda = nb01/4`, `ldc = nb1/4`, `ldb = (transposed ? nb10 : nb11)/4`, the
cuBLAS op, the batch count), the dim-2/3 strides, the group ratios, the
`16×16` pointer-kernel geometry, and the seven shape/stride assertions.

`src1_T` is derived as `ggml_is_transposed` derives it (`nb[0] != type_size`),
not taken as a flag, so the same strides decide both the assert and `ldb`.

cuBLAS stays external: `cublasSgemm`, `cublasSgemmStridedBatched` and
`cublasSgemmBatched` are audited primitives, and only their arguments are
Goose.

The assert that cost a round trip: native checks `(src1_T ? nb11 : nb10) == 4`
while `ldb` uses the *other* pair. Reversing them rejects every legitimate
shape, which is exactly what the first host gate run showed.

## Gate

The oracle is a verbatim copy of the kernel from the read-only source tree
(`reference/out-prod-kernel.cuh`, source path and `sha256 c6fe3a11…8abb94`
recorded; the kernel is defined in the `.cu`, not in a header). The host gate
restates the native branch rules and parameter formulas independently.

| check | result |
|---|---|
| pointer kernel, 6 shapes (`ne2` 1…64, `ne3` 1…19, group ratios 1…7) | 3 519 pointers, all bit-identical |
| failures / launch failures | 0 / 0 |
| host policy, 5 shapes + 3 rejection cases | 28 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| device ignores the src0 dim-2 grouping | 691 |
| device multiplies by `dps3` instead of dividing | 1 050 |
| device swaps the b strides | 1 143 |
| device swaps the c strides | 1 143 |
| device drops the float scale on the `a` pointer | 1 153 |
| device drops `i2` from the flat index | 3 486 |
| host treats a grouped src0 as the strided branch | 4 |
| host batches only `ne2` for the pointer branch | 2 |
| host defaults the src1 op to N | 1 |
| host halves the `lda` divisor | 1 |

All ten mutations are caught by the shipped gate.

## Files

- `source/device.goose` — the pointer kernel.
- `source/host.goose` — branch policy and GEMM parameters.
- `reference/out-prod-kernel.cuh` — verbatim native kernel copy with provenance.
- `tests/gpu-fixture.cu`, `tests/host-fixture.c` — the two gates.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
