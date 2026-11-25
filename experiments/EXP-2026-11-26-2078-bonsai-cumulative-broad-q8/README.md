# EXP-2026-11-26-2078 — Q8 gate/up across all Bonsai FFN layers

Retained in the cumulative experimental checkpoint for prefill research.
EXP-2049 remains the accepted checkpoint. Promotion requires combined
quality validation and resolution of the observed decode slowdown.

## Mechanism and component probe

Use the existing PQ2 × Q8 integer MMA gate/up path, with the exact full
N128 dispatch already enabled by EXP-2073. Extend activation quantization
from the last 16 FFN layers to all 64 layers. Down projections and decode
dispatch stay unchanged. Q8 activation quantization changes arithmetic.
No runtime source, executable, or device image changes in this pair.

At 1024 input rows, activation pack plus projection took 2.269–2.275 ms
across four tested projections, against 2.723–3.292 ms for FP16 partial
accumulation. The four measured
reductions were 16.5–31.1%, meeting the 10% component threshold.
Relative L2 was approximately 0.65%. Outputs were finite and the zero
row was exact. See [component results](data/component.json) for each row.

## RTX 4070, Bonsai HTTP 32K/512

One manual sequential pair on the EXP-2076 executable and CUBIN.
Identical EXP-1977 input IDs, context 36864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy, seed 123, MTP off and empty prompt cache.
The saved PrismML reference is from fork revision `910442b`.

| Run | Prefill, tok/s | Decode, tok/s | Prefill, s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A · Last 16 layers | 913.46 | 32.69 | 35.873 | 51.526 | 8932 | 7458184 |
| B · All 64 layers | 962.01 | 32.50 | 34.062 | 49.806 | 8932 | 7457516 |
| Saved PrismML | 1076.60 | 47.63 | 30.436 | 41.174 | 8288 | 7651032 |

Prefill saves 1.811 s (+5.32% throughput), and the client saves 1.720 s.
Decode is 0.58% slower in this pair. One pair leaves the repeatability
of the decode difference open. Process swap is zero. All 512 output
IDs and text match in both arms and PrismML. The prefill gap to PrismML
is 3.626 s. Reprofile the full request with the new profile before
reprioritizing the remaining work.

## Targeted quality diagnostics

CodeTrace scored 13/63 in A and 21/63 in B. Historical FP32 scored
18/63 in a separate build. A and B share 59 generated IDs before their
first divergence. Across 11 fixed-history samples, the top-1 matched
and top-10 overlap was at least nine. Maximum absolute sampled logit
delta was 0.482417. In one independent fact extraction task, both runs
answered correctly and all 47 output IDs matched. Maximum absolute
sampled logit delta there was 0.740173. These are task-specific results.
The short prompts do not exercise the long-context FP8 attention scope,
and expanded quality validation remains pending.

## Reproduce

Use the saved [EXP-2076 build](../EXP-2026-11-26-2076-bonsai-flash-fp8-pv/README.md)
and all its profile overrides. Set `BONSAI_FFN_Q8_PREFILL=last16` in A
and `BONSAI_FFN_Q8_PREFILL=1` in B. Send the exact
[EXP-1977 request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json).
The [checkpoint receipt](../bonsai-checkpoints.json) lists artifact hashes
and complete overrides. See [A/B evidence](data/results.json),
[CodeTrace](data/quality.json), and [fact extraction](data/incident.json).
