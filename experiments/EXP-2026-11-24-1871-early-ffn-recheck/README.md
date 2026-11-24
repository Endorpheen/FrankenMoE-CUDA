# EXP-2026-11-24-1871 — Recheck early FFN submission on the cumulative runtime

**REJECTED.** The unprofiled pair checks whether removing the early
routed FFN submission can eliminate a graph boundary on EXP-1863.
Earlier evidence for keeping it predates the current mailbox and
deferred prefetch fence. The EXP-1870 control trace counts 63364 graph
launches in 511 decode steps, including 20440 graphs with 11 kernel
nodes and 20440 with four. Counts describe the existing profile.

On the same binary, A uses the accepted defaults. B sets only
`GOOSE_CUDA_DECODE_EARLY_FFN=0`. Both keep split 4/count 1.
Each arm runs a fresh server, one warm-up and one scored request.
RTX 4070, Apex 32K/512, context 262144, 6290 MiB expert cache,
batch and ubatch 4096, Q4_0 KV.

| Scored measurement | Control | Deferred FFN |
| --- | ---: | ---: |
| Prefill tok/s | 3377.12 | 3339.63 |
| Decode tok/s | 83.29 | 59.37 |
| Decode ms | 6135.10 | 8607.37 |
| Client seconds | 15.92989 | 18.50991 |
| Sampled peak VRAM MiB | 8774 | 8772 |

Decode throughput fell 28.72%. The same direction was already
visible in the warm-up. This is a rejection probe, so there was no
reverse pair or adoption claim. Peak RSS was 21492692 versus
21493856 KiB. Process and system swap-out were zero. All four
responses matched the 512 token IDs and text. The full properties,
including SLRU counters, matched the accepted checkpoint.

The cumulative prefetch log reached 10000 waves and 48873 jobs in
both arms. Waiting for the predictor rose from 109.6 to 170.5 ms,
and prefetch submit time from 70.6 to 452.3 ms. These are partial
cumulative CPU timers. They do not account for the full 2472 ms
decode difference or establish a causal breakdown.

In the driver, the original early submit precedes `pf_submit()`.
The fallback expert-upload path calls the capture-aware
`cuda.upload_async` with a non-null stream. That path submits
the pending region before queueing the transfer. The batched
path calls the batch API directly. The toggle therefore does
not guarantee a uniform graph merge across layers. Measure
submission boundaries and overlap before changing those paths.
No graph-capture code or benchmark profile was modified.

Reproduce with the EXP-1863 binary and exact profile, set the
toggle only in B, and compare IDs, text, SLRU, prefill, decode,
memory and swap. See [results.json](results.json) for run IDs
and raw figures. Early FFN stays enabled in EXP-1863.
