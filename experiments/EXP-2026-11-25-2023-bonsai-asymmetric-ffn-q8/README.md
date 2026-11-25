# EXP-2026-11-25-2023 — Asymmetric early FFN Q8 prefill

**Rejected on the first independent quality fixture.** Keep EXP-2019,
including EXP-2014's late 16 layers, as the accepted Bonsai baseline.
EXP-2014 measured a prefill benefit from using signed Q8 for late
FFN gate/up. Expanding both paths to 32 or 64 layers failed its
quality gate. This experiment extends only one of the two projections
into layers 0–47, leaving the other at FP16. Both projections in
layers 48–63 retain the accepted Q8 path. Use the same kernels,
CUBIN and default DP4A decode as EXP-2019. No extra memory arena is
introduced. With mixed operators the two projections can require
separate FP16 and Q8 packs, rather than sharing one pack.

## Quality evidence

Same 440-token code-trace fixture and exact cumulative Bonsai
control. All modes use context 36864 and ubatch 1024, Q4_0 K/V,
CPU mask 0f0f, original sampling policy, no draft. The control
generates 258 IDs. In each trial, compare free response and the
first 64 positions on the same reference history. Record full
vocabulary logits at the harness sample points.

| Early scope | Free output IDs | Matching prefix | Correct cells out of 63 | Sampled same-history top-1 |
| --- | ---: | ---: | ---: | ---: |
| Accepted control, late Q8 only | 258 | 258 | 18 | Reference |
| Gate | 264 | 59 | 13 | 11/11 |
| Up | 257 | 91 | 17 | 11/11 |

The maximum sampled absolute logit difference is 0.39853 for gate
and 0.44633 for up. Same-history agreement on sparse samples does not
capture the divergent free sequence or the independent score loss.
The isolated diagnostics report 598.579 → 553.197 ms and
580.745 → 549.438 ms prefill for gate and up respectively. These
are single quality-run figures, not balanced performance results.
No 32K/512, peak process memory or shared-runtime regression
claim is made. Neither mode is enabled. No core model path changed.

This bounds projection-wide extension into all 48 early layers on
the tested cumulative runtime. It does not show that all alternative
quantizers or all subsets of early layers fail. Reuse requires the
same arithmetic, layouts and cumulative quality testing.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2023-bonsai-asymmetric-ffn-q8
out=build/bonsai-early-ffn-repro
bash "$ex/source/build.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/quality.py" gate \
    "$out/quality-gate" "$out" \
    /home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf \
    build/bonsai-integration/inventory.json build/bonsai-integration/vocabulary.gguf \
    /home/end0/AI/LlamaServer/llama.cpp/build/bin/llama-tokenize code-trace
```

Repeat with `up` and a new output directory. The opt-in diagnostic
switch is `BONSAI_FFN_Q8_EARLY=gate` or `up`. Set no option for
the accepted control. See [gate quality](data/gate-quality.json),
[up quality](data/up-quality.json), and [binary/image hashes](data/SHA256SUMS).
