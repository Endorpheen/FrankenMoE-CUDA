# Model architecture integration registry

[`model-architectures.json`](model-architectures.json) is the tracked
integration record. It stores the advertised scope, computation, formats,
KV and recurrent state, CUDA requirements, Goose changes, correctness and
quality checks, results and limitations for each scheduled architecture.
Keep candidate details unverified until the GGUF and observed operator
path support them. [Architecture scope](architecture-scope.md) tracks the
broader migration inventory.

## Starting inventory

| Model | Status | Evidence and remaining work |
|---|---|---|
| Apex-35B-V1.0 (`qwen35moe`) | Qualified checkpoint | Mandatory independent Q4_K_M and Q4_0 KV reference at 32K/512 on RTX 4070. [Apex acceptance](APEX-32K-ACCEPTANCE.md) records response and memory evidence and the open speed gate. |
| Qwen4Exp / Edge0 | Partial | [Qwen4Exp slices](C1-02-QWEN4EXP-STATE.md) and [Edge0 preflight](EDGE0-120B-ADAPTATION.md) have scoped evidence. Independent full-model acceptance is pending. |
| Maxwell-V1-Flash | Historically qualified | [The production audit](PRODUCTION-CPP-AUDIT-20261120.md) records nine historic responses and the factory gap. A reproducible independent control is required before another architecture is accepted. |
| Inkling-2-Mini | Planned | Next after Apex speed recovery. Preferred AD-IQ4_XS. Pin the artifact before classifying its effective GGUF layout, quantized operations or CUDA/KV requirements. |

## Required acceptance procedure

Routine optimization before Apex speed recovery runs the Apex 32K/512
reference on RTX 4070. Run another model's checks for changes that affect
its path directly. The full registry remains mandatory on architecture
acceptance and general releases. Keep missing references recorded while
continuing Apex optimization.

1. Pin each model artifact, request and baseline executable by SHA-256.
   Record effective context, batch/ubatch, cache formats and budget,
   CPU limits, GPU and Goose selectors. Add the necessary controlled
   differential fixtures and compare generation to the architecture's
   actual quality requirements. Link the evidence from the record.
2. Build one independent Goose runtime. Reuse common functionality for
   different models and GPUs. Add explicit capability and tensor guards
   for specializations and retain the supported general path. Check the
   [GPU policy](NVIDIA-ARCH-POLICY.md) before making a hardware claim.
3. Run the mandatory local gate against the candidate. It executes every
   registered reference through the existing serial capture lifecycle.
   Use a new report directory and retain its receipt with the candidate's
   exact binary hash.

```bash
FG_MODEL_CANDIDATE=/path/to/gooseinfer-infer \
FG_MODEL_REPORT=/path/to/new-acceptance-report \
  bash tests/run.sh models architecture-regressions
```

The gate verifies model, request and baseline hashes, live arguments and
properties, full model residency and the process maps. Each benchmark
uses A–B–B–A and records both throughput medians, peak process RSS,
process and card VRAM, process swap and system swap-out. The existing
2% paired performance limit is applied independently to prefill and
decode. The process memory peak must stay within the baseline maximum
plus its observed spread. All measured differences remain in the
receipt. Any swap activity invalidates the comparison.

An intact response can differ from the pinned one. A new fingerprint
requires reviewed evidence of computation, state and quality for that
exact candidate output before it can be listed in `reviewed_outputs`.
The gate checks all token IDs, probability values, timing denominators
and response framing automatically. Do semantic review against the
actual task and archive it in the referenced evidence file.

For Apex, `FG_APEX_GGUF`, `FG_APEX_REQUEST` and `FG_APEX_BASELINE` may
relocate the three pinned artifacts. The hashes remain mandatory. The
local launcher measures real models and requires the registered GPU.
CI checks the schema and policy fixtures without model files. A CI
schema pass has no runtime acceptance value.

The receipt fails if a registered reference cannot run, an earlier
qualified architecture lacks a reproducible reference, or any comparison
fails. Preserve raw results, fix or isolate the regression and rerun its
checks before accepting. This checks regressions against the independent
checkpoints. The stronger [Optimized Hybrid speed target](OPTIMIZED-HYBRID.md)
remains part of the Apex plan.

## Initial live gate check

EXP-1602 ran the four Apex requests against the same EXP-1601 binary
in both roles. The response fingerprints were identical. Prefill
medians were 1,130.659 and 1,133.606 tok/s, decode 28.169 and 28.234
steps/s, and peak process VRAM 7,272 MiB in both arms. The final
control saw 50,867 pages of system swap-out. The gate rejected the
series despite zero process swap and acceptable rate and memory
samples. The missing reproducible Maxwell reference was recorded
separately. Raw captures and the receipt are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/model-registry-exp1602`.
