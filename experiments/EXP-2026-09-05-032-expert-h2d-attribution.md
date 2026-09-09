# EXP-2026-09-05-032 — Expert H2D attribution

Status: ACCEPTED_MEASUREMENT

## Hypothesis

The 22.2 GiB of CUDA host-to-device traffic measured in EXP-031 is primarily selected MoE expert weights rather than activations, KV state, or generic scheduler copies.

## Change

An isolated copy of the integration tree added the env-gated `GGML_CUDA_H2D_TRACE` diagnostic at the CUDA backend H2D submission point. It prints tensor name, offset, and byte count. The working integration tree and production build were not changed.

Earlier EXP-032 scheduler-level probes emitted no records because expert tensors are marked as graph inputs. The CUDA backend probe is the authoritative point. Its final version records a monotonic timestamp for each H2D submission.

## Method

- One server instance on port 8092.
- Same model, quantization, CPU-MoE routing, MTP split draft, and 12 threads as the active configuration.
- `-c 32000` was used only for diagnostic VRAM headroom; the fixed 395-token prompt is unaffected by the context limit.
- Fixed request `benchmarks/inputs/exp028-prefill.json`, SHA-256 `9fc8652f5ab616073d95ae301afa80cbefe7e604e6a59532fd5f11d716f14184`.
- `n_predict=0`: prefill only, no generated response tokens.
- Server was stopped immediately after the response.

## Result

| Metric | Value |
| --- | ---: |
| Prompt tokens | 395 |
| Completed-request prefill time | 4029.308 ms |
| Completed-request prefill throughput | 98.032 tok/s |
| H2D calls | 17,886 |
| H2D volume | 22,160.56 MiB |
| H2D submission span | 3,634.692 ms |
| Expert-weight H2D share | 100% of calls and bytes |
| Mean transfer | 1,268.7 KiB |
| Median transfer | 900 KiB |
| P90 / P99 transfer | 2,700 / 5,400 KiB |
| `ffn_gate_exps` | 5,962 calls; 5,920.53 MiB |
| `ffn_up_exps` | 5,962 calls; 5,920.53 MiB |
| `ffn_down_exps` | 5,962 calls; 10,319.50 MiB |

## Correctness and memory

The HTTP request completed normally. This was a trace-only isolated build and made no production-code, model, routing, quantization, or memory-residency change. The diagnostic server was terminated cleanly. The first timestamped H2D line follows the server's `processing task` line; the last falls before `prompt processing` completes. The 3,634.692 ms H2D span fits within the measured 3.78 s bulk-prefill window.

## Decision

ACCEPTED as measurement infrastructure. It conclusively attributes the bulk-prefill transfer bottleneck to many small selected-expert weight transfers. It does not claim a speed improvement.

## Next experiment

Inspect and prototype a minimal two-stage expert transfer/compute overlap for a single layer boundary. It must preserve router-selected IDs and compare against the current fixed prefill baseline before any wider integration.
