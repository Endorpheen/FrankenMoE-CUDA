# EXP266: CUDA Q2_K dequantization to half and bfloat16 in Goose

`dequantize_q2_K<dst_t>` for `half` and `nv_bfloat16`. EXP250 covered the F32
destination.

## Verdict

**OFF_PATH** — bit-exact against both native instantiations; the fixed 32K/128
Apex profile never selects Q2_K through these convertors. No performance claim,
shipped DSO unchanged.

## Scope transferred

EXP250's arithmetic per destination: 64-thread (n = tid/32, l = tid-32*n,
is = 8*n + l/16), base `i*QK_K + 128*n + l`, dm read at the **end** of the block
(80/82), four 2-bit fields from one qs byte, a 4-bit scale/min per scales byte,
and `dall*sc*q - dmin*min`. Only the store differs.

Layout: `block_q2_K` is 84 bytes — `scales[16]` at 0, `qs[64]` at 16, `dm` at 80
(`d` at 80, `dmin` at 82).

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
| `dmin` offset 82 -> 80 (reuses `d`) | 4 |
| `qs` offset 16 -> 17 | 4 |
| second field shift 2 -> 1 | 4 |
| BF16 store uses the half conversion | 4 |
| destination `128*n` -> `127*n` (F16 body) | 4 |

No defect was found on the first build; the end-of-block dm offset was already
carried over correctly from EXP250.

## Not integrated

Same rule as EXP245–265: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

F16/BF16 for Q3_K, the IQ family and MXFP4; the activation quantizer and MMQ
stay blocked on the EXP180 emitter address typing.
