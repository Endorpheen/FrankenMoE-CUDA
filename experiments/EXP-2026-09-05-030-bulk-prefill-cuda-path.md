# EXP-2026-09-05-030-bulk-prefill-cuda-path

Status: `ACCEPTED` as measurement infrastructure.

## Question

Does the main prompt prefill reach MoE in small CPU chunks, and can it safely be enlarged?

## Method

The existing `GGML_MMID_TRACE` diagnostic was enabled in an isolated build. It logs the CUDA implementation selected for each `MUL_MAT_ID` and its token dimension. One deterministic 395-token request was sent with the usual MTP profile; this was a diagnostic run, not a speed comparison.

The server used a 32K context only because the diagnostic build fit the available VRAM in that state. It does not alter positions or routing for a 395-token request.

## Result

The first 391 prompt tokens were processed through CUDA MMQ as one bulk prefill batch. There were 144 CUDA `MUL_MAT_ID` calls:

| Weight type | Calls | Tokens per call |
| --- | ---: | ---: |
| IQ4_NL | 48 | 391 |
| IQ3_S | 2 | 391 |
| IQ2_S | 94 | 391 |

The final four prompt tokens were processed separately. This is intentional server behavior: when context checkpoints are enabled, the server retains a four-token tail so it can make a safe checkpoint for MTP rollback and context-cache management.

## Decision

The premise that bulk prefill was fragmented into small CPU pieces is false. The CPU IQ2_S trace in EXP-029 observed only the intentional four-token checkpoint tail; it did not observe the 391-token CUDA MMQ bulk prefill.

Changing `-b` or `-ub` cannot enlarge this already-large primary batch. Disabling checkpoints with `-ctxcp 0` would remove the tail but changes context rollback and cache safety, so it is not an acceptable default optimization. Future prefill work should target CUDA MMQ or another proven bulk-prefill bottleneck.

## Reproduction

- Isolated source and build: `work/llama.cpp-exp029-group-trace`, `build/exp029-group-trace`.
- Environment flag: `GGML_MMID_TRACE=1`.
- Raw log: `/tmp/exp030-server-1788632150.log`.
- Fixed request: `benchmarks/inputs/exp028-prefill.json`.
