# EXP-2026-11-14-581 — type selection and imatrix flags (P7-04 S1a)

Native counterpart: `llama_quant_compute_types`, `tensor_requires_imatrix` and
`ggml_quantize_requires_imatrix` in `llama-quant.cpp` / `ggml.c`. Base commit:
`4dac50f2`.

## What this slice covers

`source/quant.goose` (695 lines, 3 exports) ports the type-selection policy:

- `qt_compute` — the per-tensor ftype choice for every supported ftype, with
  `n_gqa`, `n_expert`, `n_layer`, tensor kind, overrides (`token_type`,
  `output_type`, `quantize_output`, `only_copy`, `pure`) and the `n_fallback`
  counter;
- `qt_tensor_requires_imatrix` — the tensor-name rule for all 43 types;
- `qt_ggml_quantize_requires_imatrix` — the ggml-level rule for all 43 types.

The port uses the native block table. Two conformity fixes were needed against
the hand-over draft, both inside the decision data, with no architecture change:
`qt_block` had 32 for NVFP4/Q1_0/Q2_0 where native uses 64/128/64, which changed
the output branch and the fallback; and `C_FFN_DOWN`/`Q3_K_M` applied the Falcon
split while native gives Q4_K for every non-Falcon model (Falcon is owned by S2).

## Gate

`tests/build.sh` builds the port with `--standalone`, compiles the generated C
at `-O0` for correctness and links `tests/fixture.cpp` — which includes the real
`llama-quant.cpp` — against the prebuilt llama libraries. No weights.

```
FG584_QUANT cases=734 values=90461 failures=0
```

Coverage: all 35 supported ftypes (NVFP4 excluded, see limits) × n_gqa
{1,2,4,8} × n_expert {2,4,8} × n_layer {1,2,8,16,32} × six tensor shapes; every
override; `n_fallback` in every case; `tensor_requires_imatrix` over 43 types ×
36 ftypes × 8 names; `ggml_quantize_requires_imatrix` over all 43 types.

## Mutations

`tests/mutate.py --half 1|2` holds 12 textual mutations in two halves, each
checked with `goose check`, rebuilt at `-O0` and required to fail the gate.

```
python3 tests/mutate.py --half 1   # 6/6 caught
python3 tests/mutate.py --half 2   # 6/6 caught
```

## Recorded limits

- `has_imatrix=true` has no native oracle in this revision, so the imatrix
  branches are not compared.
- NVFP4 has no native default type in this revision and is excluded from the
  ftype grid.
- Q1_0/Q2_0 shapes not divisible by the block size make native throw an
  exception that Goose does not reproduce.
- Architecture branches and regex overrides belong to S2 (download/quant CLI
  policy), not to this slice.
