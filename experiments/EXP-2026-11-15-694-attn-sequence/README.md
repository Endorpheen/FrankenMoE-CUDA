# EXP-2026-11-15-694 — the attention layer's real sequence: q/k per-head norms and IMRoPE

Base: `2969254e`. No model run, no GPU.

## 1. What was wrong

EXP-688b reported `wiring_differs=0` for the attention layer, and that number was
true. It was also not enough. Reading `qwen35moe.cpp:311-345` shows the native order is

```
Qcur  = view_3d(wq)  -> rms_norm(attn_q_norm) -> multiply -> IMRoPE
Kcur  = reshape_3d   -> rms_norm(attn_k_norm) -> multiply -> IMRoPE
gate  = view_3d(wq, offset = elem*head) -> cont_2d
attn  = flash_attn(roped q, roped k, v, scale)
cur   = attn * sigmoid(gate) -> wo -> residual
```

The gate had **no per-head RMS norm on q or k**, and used `ggml_rope_ext` instead of
`ggml_rope_multi`. Both arms omitted them, so both agreed. That is the specific blind
spot of a differential gate: it proves the two arms are the same graph, not that the
graph is the right one. A wiring check needs its sequence read from the model, and
this card is the correction.

## 2. Result after the fix

```
cmp normed   n=2048 differs=0      cmp kr       n=512  differs=0
cmp qg       n=8192 differs=0      cmp attn     n=4096 differs=0
cmp k4       n=512  differs=0      cmp g4       n=4096 differs=0
cmp v4       n=512  differs=0      cmp gs       n=4096 differs=0
cmp q_norm   n=4096 differs=0      cmp gated    n=4096 differs=0
cmp k_norm   n=512  differs=0      cmp proj     n=2048 differs=0
cmp qr       n=4096 differs=0
FG_APEX_LAYER ... wiring_differs=0 vs_ggml=0 PASS
```

Thirteen intermediates plus the output, all zero.

## 3. The parameters, from the file

| key / tensor | value | consequence |
|---|---|---|
| `blk.3.attn_q_norm.weight` | [256, 1] | per-head RMS weight on q |
| `blk.3.attn_k_norm.weight` | [256, 1] | per-head RMS weight on k |
| `rope.dimension_count` | 64 | `n_rot` |
| `rope.dimension_sections` | [11, 11, 10, 0] | IMRoPE bands |
| `rope.freq_base` | 1e7 | base frequency |
| `layer_norm_rms_epsilon` | 1e-6 | both norms |

`ggml_rope_multi` takes the sections; `ggml_rope_ext` has no such argument, so the old
call could not have been equivalent however the other parameters were set.

## 4. What moved into Goose

`q35_rope_multi` in `goose/model/qwen35moe.goose`, with `fg158_rope_multi` as a
translation-only primitive. The sections and `n_rot` are a property of the Qwen3.5-MoE
family, so they belong to that module rather than to the gate.

## 5. The lesson, recorded rather than glossed

"Both arms agree" is necessary and not sufficient. A differential gate must have its
sequence established from the model source, and the sequence itself needs its own
check against the source - here, three separate omissions survived a passing gate.
The next layer-level gate should start by restating the native sequence line by line
before building anything.

## 6. Cost

One arch-unit build (122 modules, 1071 symbols) for the new rope export, plus gate
compiles.
