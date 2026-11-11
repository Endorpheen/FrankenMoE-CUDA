# EXP263: CUDA Q4_K dequantization to half and bfloat16 in Goose

`dequantize_q4_K<dst_t>` for `dst_t = half` and `dst_t = nv_bfloat16`, reached
through `ggml_get_to_fp16_cuda` / `ggml_get_to_bf16_cuda`. EXP182 covered the
F32 destination; this is the first non-F32 destination module.

## Verdict

**OFF_PATH** — bit-exact against both native instantiations, but the fixed
32K/128 Apex profile never selects Q4_K through the F16/BF16 convertors (Q4_K_M
runs through MMQ). No performance claim, shipped DSO unchanged.

## Scope transferred

The arithmetic is the F32 module's, reproduced for each destination: the 8x4
thread decomposition, the row base, the packed half2 scale/min unpack, both
6-bit pairs from the interleaved scales array, the nibble split and the affine
`d*q - m`. Only the final store differs.

Conversion semantics, read from `convert.cuh`: `ggml_cuda_cast<half>(float)`
falls through to `float(x)`, so the assignment performs the round-to-nearest-even
float→half conversion; `ggml_cuda_cast<nv_bfloat16>(float)` is
`__float2bfloat16(float(x))`. The Goose stores use `__float2half` and
`__float2bfloat16`, the same operations.

Layout: `block_q4_K` is 144 bytes — `ggml_half d` at 0, `dmin` at 2,
`uint8_t scales[12]` at 4, `uint8_t qs[128]` at 16. `QK_K = 256`.

## Gate

| check | result |
|---|---|
| F16 destination, 4 cases (1/4/64/256 blocks) | 83 200 values bit-identical |
| BF16 destination, 4 cases | 83 200 values bit-identical |
| total values compared | 166 400 |
| failures / launch failures | 0 / 0 |

Each destination runs a native kernel and a Goose kernel on the same random block
bytes; the gate is `uint16` equality of the whole destination.

## Mutations

| mutation | failures |
|---|---:|
| `scale_min` m uses `q[j+4]` -> `q[j+3]` (shared helper) | 8 |
| q offset `32*il` -> `16*il` (F16 body) | 4 |
| second-half offset `+32` -> `+16` (F16 body) | 4 |
| BF16 store uses the half conversion | 4 |
| affine sign `-m1` -> `+m1` (F16 body) | 4 |

No defect was found on the first build.

## Not integrated

Same rule as EXP245–262: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

F16/BF16 destinations for Q2_K, Q3_K, Q5_K, Q6_K, the IQ family and MXFP4; then
the activation quantizer blocked on the emitter address typing (EXP180) and the
MMQ/MUL_MAT families.
