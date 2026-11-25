# EXP-2026-11-25-1996 — Weight reuse across Q8 activation and residual products

**REJECTED for default use.** The correction restores the target ID
at the EXP-1994 divergence and recovers its lost program-trace cells.
It still changes ID selection on the same longer question at a later
position. The FP16 MMQ control remains the default.

## Component

Round activations to the control's F16 before packing. Represent each
128-value block with a Q8 main term and a Q8 rounding residual. The
residual scale is the main scale divided by 254. The two integer products
share the same decoded PQ2 weight tile. Combine them inside the same CTA
and apply the block scales in F32. Weight storage is unchanged.
The runtime overlays quant scales on the existing attention scratch
between its sequential operations on the same stream.

| Matrix / tokens | FP16, ms | Plain INT8, ms | Q8 residual, ms | Relative L2 vs FP16 |
| --- | ---: | ---: | ---: | ---: |
| QKV tail / 33 | 0.239 | 0.115 | 0.180 | 0.00266% |
| QKV / 257 | 1.045 | 0.462 | 0.714 | 0.00267% |
| QKV / 1024 | 3.329 | 1.475 | 2.209 | 0.00268% |
| Attention output / 1024 | 2.070 | 0.933 | 1.389 | 0.00272% |
| FFN up / 1024 | 5.581 | 2.418 | 3.664 | 0.00266% |
| FFN down / 1024 | 5.808 | 2.585 | 3.877 | 0.00374% |

All six shapes pass the independent CPU sampled arithmetic test and
the residual relative L2 gate below 0.01%. This figure measures each
projection in isolation. CUDA timing includes both pack operations and
matrix computation. The kernel uses 69 registers and 28,160 bytes shared
memory with no spills. Global workspace allocation stays unchanged.

## Short performance

4097/16, first 4097 raw IDs from EXP-1977, context 36864, ubatch 1024,
Q4_0 K/V, greedy with EOG suppressed and MTP off. A–B–B–A with the same
GDN and Flash path. The two pairs save 1.654 and 1.675 s prefill.
All 16 IDs coincide with FP16 and the saved matched PrismML reference.
Decode is about 0.975 s in both modes. Both allocate 1,720,400,640 bytes
GPU workspace. Peak process RAM and VRAM were outside this short block.

One separate candidate Nsight capture places 3790.708 ms in residual
MMQ, 93.107 ms in activation packing and 5265.524 ms in the total
prefill GPU span. Decay/beta takes 357.586 ms and Flash tiles
199.031 ms. The paired short prefill leaves about 4.446 s to remove
to reach 5000 tok/s at that input length. This is an experimental
measurement, not a production improvement or a 32K extrapolation.

## Multi-request quality

Same GGUF template and four exact tasks as EXP-1994. The extraction case
is correct and identical in both modes. Ledger and schedule answers
are identical but wrong in both. Program trace native free generations
have 258 and 262 IDs, with 195 shared before the first difference.
Both score 18/63 independently checked cells. Plain INT8 previously
scored 14/63 and diverged after 59 tokens. This correction therefore
recovers that measured cell-score regression, but exact sequence
equivalence remains unproven.

Fixed-history full-vocabulary replay at the original and new
boundary records these top decisions:

| Output position | FP16 choice | Q8 residual choice | Maximum absolute logit difference |
| --- | ---: | ---: | ---: |
| 60 | 15 | 15 | 0.2161 |
| 196 | 16 | 17 | 0.2823 |

At position 196 both models see the same 195 output tokens. FP16
assigns 18.9089 to ID 16 and 18.8187 to ID 17; the trial assigns
18.7016 and 18.8476. Normal periodic samples through the first 64
steps share top-1 on all four tasks, illustrating why targeted
late-history diagnostics are necessary. No 32K/512 model run was
made after the short token gate failed. Apex and Inkling code
and CUDA images remain unchanged.

Before changing activation precision again, compare layer-by-layer
state and Q4 row writes on the code-trace prefill. Determine where the
small projection error first becomes a discrete change. Preserve
the exact FP16 data path as a control and continue exact optimizations
when equivalent logits have not been established.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1996-bonsai-pq2-q8-residual
base=experiments/EXP-2026-11-25-1994-bonsai-pq2-int8
out=build/bonsai-residual-repro
model="$HOME/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf"
bash "$exp/source/build.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-residual.py" \
    build/bonsai-integration build/bonsai-integration/inventory.json \
    "$out/pq2-residual.cubin"
OPENBLAS_NUM_THREADS=1 python3 "$base/source/check-quality.py" \
    "$out/quality" "$out/quality-fixture" "$model" \
    build/bonsai-integration/inventory.json build/bonsai-integration/vocabulary.gguf \
    "$HOME/AI/LlamaServer/llama.cpp/build/bin/llama-tokenize"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-history.py" \
    "$out/quality-fixture" "$model" "$out/quality/code-trace" 195 17
```

Use EXP-1994 settings with `bonsai-mmq-residual` and `pq2-residual.cubin`
for the short A/B. In this experimental executable only,
`BONSAI_PQ2_MMQ_INT8=1` selects the two-term residual path.
Raw captures and full logits are local under
`build/bonsai-mmq-residual-1996-runtime`.

[Component data](component.json). [Short A/B](results.json).
[Quality](quality.json). [Old boundary](history-59.json).
[New boundary](history-195.json). [Updated profile](profile.json).
