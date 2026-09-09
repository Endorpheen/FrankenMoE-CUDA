# EXP-2026-09-05-031-cuda-prefill-transfer-profile

Status: `ACCEPTED` as measurement infrastructure.

## Question

Where does bulk MoE prefill spend time: CUDA computation or transfers of CPU-resident expert tensors?

## Method

One prefill-only deterministic request was captured with Nsight Systems. `n_predict` was set to zero, so no decode result is included. The server otherwise used the MTP profile with CPU MoE and EHS disabled. A 32K context was used only because the isolated diagnostic build fit the current VRAM state.

This is a trace, not a throughput benchmark: profiling overhead changes timing. Its purpose is attribution.

## Result

The server processed 395 prompt tokens in 4.016 s; the main 391-token bulk batch took 3.760 s.

Within the prompt interval:

| Item | Measurement |
| --- | ---: |
| Host-to-device copies | 17,975 calls, 22,207.85 MiB |
| H2D time on CUDA timeline | 1,661.18 ms |
| All CUDA kernel time, summed | 438.91 ms |
| IQ2_S MMQ kernels | 128.91 ms |
| IQ4_NL MMQ kernels | 91.64 ms |
| MoE expert grouping helpers | 15.97 ms |

The request's bulk prefill uses CUDA MMQ with 391 tokens. EXP-032 then added timestamps directly at the CUDA H2D submission point: all 17,886 copies occur in a contiguous 3,634.692 ms interval after request launch and inside the 3.780 s bulk-prefill window. This validates the original attribution.

## Decision

P1 is confirmed. The next implementation experiment must test one focused mechanism to overlap or reduce these transfers, beginning with asynchronous staged prefetch of the next expert tensor. It must remain isolated, preserve deterministic output, and be compared against the current baseline.

## Reproduction

- Isolated build: `build/exp029-group-trace`.
- Nsight report: `/tmp/exp031b-cuda-prefill-1788632387.nsys-rep`.
- Extracted report text: `/tmp/exp031b-stats.txt`.
- Fixed request: `benchmarks/inputs/exp028-prefill.json`, with `n_predict` changed to `0`.
