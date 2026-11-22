# EXP-1695 — Overlap completion probability reporting with decode

The [full decode differential](../EXP-2026-11-22-decode-parity-profile/README.md)
isolates 2.293 seconds of CPU work from requesting the probability
rows. The producer in the composed Goose host still chooses the
token in its original order. A separate private Goose worker
computes the same probability routine on an immutable copy of raw
logits. The C adapter owns a three-entry queue and scratch buffers.
Joining the worker completes the response before any rows are read.
Drain time is included in decode measurement and in client time.

The focused ASan/UBSan probe verifies queue reuse, early abort
cleanup, invalid ID rejection and bitwise rows at vocabularies of
37 and 248,320. The production inference server uses one serialized
request executor. The worker is a separate generated module with its
own arena and is not called concurrently by multiple workers.

## Goose at original n_probs=5

The executable and all runtime options are identical between the
two arms, except `GOOSE_CPU_PROBABILITY_PIPELINE=0|1`. The EXP-1694
warm-aware profile uses 6,290 MiB SLRU, a 210 MiB GPU workspace,
a 384 MiB activation arena, and full host pin. Both requests in
each process use 32,768 input, 512 output, 4,096/4,096 batches,
and zero reused prompt tokens. The second response is measured.

| Measure | Inline reports | CPU worker |
|---|---:|---:|
| Prefill, tokens/s | 2,995.10 | 3,004.67 |
| Decode, tokens/s | 33.76 | 36.62 |
| Decode, seconds | 15.137 | 13.952 |
| Client, seconds | 26.194 | 24.971 |
| Peak process VRAM, MiB | 8,766 | 8,766 |
| Peak process RSS, KiB | 21,489,792 | 21,493,256 |
| Process swap, KiB | 0 | 0 |
| System swap-out after readiness, pages | 0 | 0 |

Both the warm-up and measured responses match the control's
content, all 512 IDs and every probability row bitwise. The
SLRU counters also match. The control incurred 6,082 and the
candidate 3,290 system swap-out pages during startup. The
performance checkpoint is an ordinary, warmed A/B observation
with the numerical and resource conditions above. The immutable
EXP-1694 checkpoint remains the rollback.

## One-off Optimized Hybrid probability comparison

The frozen EXP-1597 hybrid and its shared libraries pass their
SHA-256 manifest. The same process first warms at `n_probs=5`,
then measures `n_probs=0` and `n_probs=5`. Both measured
requests use the original model, full host pin, 5,632 MiB GPU
cache, 32K/512, 4,096/4,096 and zero reused prompt tokens.

| Mode | Prefill, tokens/s | Decode, tokens/s | Decode, seconds | Client, seconds |
|---|---:|---:|---:|---:|
| No probability rows | 2,946.07 | 71.17 | 7.180 | 18.315 |
| Five probability alternatives | 2,953.60 | 66.33 | 7.704 | 18.818 |

The 512 IDs and text match. Full lifecycle system and process
swap are zero. Peak VRAM is 8,850 MiB and peak RSS 21,870,240 KiB.
Removing the report yields +4.84 tok/s and saves 523.558 ms
decode. This is a single controlled diagnostic, and does not
alter the acceptance profile or add hybrid runs to future
Goose experiments.

## Artifacts

The exact binary, link archive, source and raw responses are
under `build/exp1695-probability-pipeline` with `SHA256SUMS`.
Binary SHA-256:
`e1fbe0d6e49eae229e49a63d9d691c0f858009d885d431f3fb0a6e695691523d`.
The hybrid logs are in
`build/exp1694-prefill-workspace-budget/hybrid-probability-cost`.

For reproduction, apply `host-pipeline.patch` to the EXP-1668 composed
host unit. Build `worker-entry.goose` after the EXP-1668 sampler
module with its original exports made private. Keep the original
sampler bodies. Emit C and compile with the pinned Fried Goose and
GCC 13 `-O1`, renaming the private worker's `goose_init` and
`gg_init_globals` to `fg_probability_init` and
`fg_probability_globals`. Replace the composed `main.o` in a copy
of the EXP-1694 archive, append the private worker and C adapter
objects, and link the normal CLI with pthread, libc and libm.
