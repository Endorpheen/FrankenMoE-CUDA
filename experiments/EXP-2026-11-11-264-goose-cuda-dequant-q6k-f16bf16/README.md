# EXP264: CUDA Q6_K dequantization to half and bfloat16 in Goose

`dequantize_q6_K<dst_t>` for `half` and `nv_bfloat16`, reached through
`ggml_get_to_fp16_cuda` / `ggml_get_to_bf16_cuda`. EXP248 covered the F32
destination; EXP263 established the two conversion semantics.

## Verdict

**OFF_PATH** — bit-exact against both native instantiations, but the fixed
32K/128 Apex profile never selects Q6_K through these convertors. No
performance claim, shipped DSO unchanged.

## Scope transferred

The F32 module's arithmetic reproduced per destination: 64-thread decomposition
(ip/il/is), base `i*QK_K + 128*ip + il`, the low nibble combined with the 2-bit
high part from `qh`, the signed per-16 scale and `d*sc*(q-32)`. Only the store
differs: `__float2half` (the same round-to-nearest-even conversion as the
implicit float→half assignment) and `__float2bfloat16`.

Layout: `block_q6_K` is 210 bytes — `ql[128]` at 0, `qh[64]` at 128,
`int8_t scales[16]` at 192, `ggml_half d` at 208. `QK_K = 256`.

## Gate

| check | result |
|---|---|
| F16 destination, 4 cases (1/4/64/256 blocks) | 83 200 values bit-identical |
| BF16 destination, 4 cases | 83 200 values bit-identical |
| total values compared | 166 400 |
| failures / launch failures | 0 / 0 |

## Mutations

| mutation | failures |
|---|---:|
| `qh` high-bit shift 2 -> 1 (F16 body) | 4 |
| scale index `+2` -> `+1` (F16 body) | 4 |
| third value offset `+64` -> `+32` (F16 body) | 4 |
| BF16 store uses the half conversion | 4 |
| offset constant `-32` -> `-31` (F16 body) | 4 |

No defect was found on the first build.

## Not integrated

Same rule as EXP245–263: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

F16/BF16 for Q2_K, Q3_K, Q5_K, the IQ family and MXFP4; the activation quantizer
and MMQ stay blocked on the EXP180 emitter address typing.
