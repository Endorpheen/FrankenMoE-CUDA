# EXP-2026-09-05-029-iq2s-group-distribution

Status: `INCONCLUSIVE`.

## Hypothesis

The IQ2_S fused prefill idea is only worth revisiting if the router commonly sends several prompt tokens to the same expert in one `MUL_MAT_ID` operation.

## Method

An isolated copy of the integration tree records a compact histogram immediately after rows are grouped by selected IQ2_S matrix. One 395-token fixed prompt was processed with generation limited to one token. Decode rows are excluded.

The context reserve was 32K instead of the daily 64K profile solely to fit the trace build in currently available VRAM. The prompt length is 395 tokens, so this changes neither token positions nor router decisions.

## Result

Across 94 IQ2_S `MUL_MAT_ID` invocations there were 2,594 non-empty expert groups and 3,760 routed rows, or 1.4495 rows per group on average.

This result was initially interpreted incorrectly. The model routes 10 experts per token, so each invocation with 40 routed rows represents four tokens. The server log shows that the main 391-token prefill completed first; the 94 traced operations occur only in the separate four-token checkpoint immediately before prompt completion. This tail exists so the server can retain a safe context checkpoint for MTP rollback and cache management.

| Group size | Groups | Share |
| --- | ---: | ---: |
| 1 | 1,786 | 68.85% |
| 2 | 550 | 21.20% |
| 3-4 | 258 | 9.95% |
| 5+ | 0 | 0.00% |

Thus 90.05% of groups in the four-token checkpoint tail have one or two rows, and none has five or more. This does not measure the bulk prefill, so it says nothing conclusive about reuse there.

## Decision

The earlier conclusion is withdrawn. EXP-029 establishes why the final four tokens are processed separately, but it does not establish the group-size distribution of bulk prefill. The safe next diagnostic must trace the actual backend that executes the 391-token bulk prefill, rather than only CPU IQ2_S operations.

## Reproduction

- Isolated source: `work/llama.cpp-exp029-group-trace`.
- Isolated build: `build/exp029-group-trace`.
- Environment flag: `GGML_CPU_IQ2S_GROUP_TRACE=1`.
- Raw server log: `/tmp/exp029-server.log`.
- Fixed request: `benchmarks/inputs/exp028-prefill.json` with `n_predict` changed to `1` for the trace request.
