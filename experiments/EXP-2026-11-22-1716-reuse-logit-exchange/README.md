# EXP-1716 — Reuse the host exchange allocation in decode

One decode evaluation exchanges one token and a vocabulary-sized
logit row with nested Goose exports. The exchange must live outside
the Goose arena because the exports can reuse arena slots. The host
now reuses its allocation for one-token decode calls. The thread
owns the buffer and frees it at thread exit. Prefill, a nested
borrow, or a failed key/allocation uses the original allocation path.
The copies and sampling math are unchanged.

The clean source recompiled to the original EXP-1713 model-host object
byte for byte before applying this change. The candidate uses the
same archive with only `model-host.o` replaced and that source is
also committed directly.

## Full warmed Apex pair

RTX 4070, Apex Q4_K_M, 32,768 input / 512 output tokens, default API
without probability rows, batch and microbatch 4,096, Q4_0 KV, context
262,144, 6,290 MiB expert bank and 210 MiB temporary workspace.
Each unprofiled arm completed one full warm-up and one measured
request on the same process, with zero prompt reuse.

| Metric | Fresh EXP-1713 | EXP-1716 |
|---|---:|---:|
| Prefill, tok/s | 3007.91 | 3004.73 |
| Decode, tok/s | 45.31 | 45.88 |
| Decode, ms | 11277.723 | 11137.883 |
| Client, s | 22.262814 | 22.133656 |
| Peak process VRAM, MiB | 8766 | 8766 |
| Peak RSS, KiB | 21490376 | 21490308 |
| Process swap, KiB | 0 | 0 |
| System swap-out, pages | 0 | 0 |

The warm decode saves 139.841 ms and the client saves 129.158 ms.
Prefill takes 11.551 ms longer in the measured pair. On the cold
request the candidate decode is faster and total client time is
44.549 ms longer. Both requests match all 512 IDs, full response
text and every SLRU counter. The control is distinct from the
historical 45.79 tok/s measurement on EXP-1713.

Keep EXP-1716 as the accumulated decode candidate and preserve
EXP-1713. Raw data and checked binaries are stored under
`build/exp1716-reuse-logit-exchange`, candidate SHA-256
`57429091087e7f14e328146c208b468038fe5cf083ceedb82f19f8360e7365a8`.
