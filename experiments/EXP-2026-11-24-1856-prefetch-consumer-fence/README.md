# EXP-2026-11-24-1856 — Deferred prefetch fence research checkpoint

This checkpoint extends the accepted route mailbox. The decode graph permits
packet submission and the shared FFN to run before the routed expert weights
are needed. Compact routed decode waits at the existing up/gate and down
consumer fences. Outstanding and new DMA retain their order on the expert
stream. The synchronous preparation barrier stays in place for other paths.
Set `GOOSE_DEFER_PREFETCH_FENCE=0` to restore it for compact decode.

| Arm | Prefill tok/s | Decode tok/s | Client seconds |
| --- | ---: | ---: | ---: |
| A1 | 3245.354 | 82.859 | 16.350486 |
| B1 | 3240.427 | 83.685 | 16.305756 |
| B2 | 3251.403 | 83.393 | 16.296404 |
| A2 | 3247.741 | 82.923 | 16.345266 |
| Default | 3239.967 | 83.540 | See results.json |

The paired mean improved from 82.891 to 83.539 decode tok/s. All ten
responses matched the original 512 IDs and text and all five properties
reports matched the accepted mailbox byte for byte. Peak VRAM was
8774 MiB, process swap and system swap-out were zero. The small prefill
difference is within the single-request spread.

The tested executable is `frankenmoe-infer`. It uses the same cumulative
archive as route mailbox with only the CUDA adapter replaced. Build with
GCC 13 and the same flags as the mailbox relink recipe. `argv.txt` and
`env.txt` are the final measured settings. `validation/ab-infer` uses the
opt-in experimental flag for the A/B pairs. The final binary enables
both optimizations by default.

The predictor probes retained the original logits and ID order in all
30 cases per variant. The warp top-eight, 128-thread fold, fixed width,
two rows per block and early cutoff did not justify promoting a new
predictor. Logs and source are retained under `predictor-probes/`.

The tested final executable SHA-256 is
`c935a76fa853d02a86b11d27ad3438a6ce29606518716d0a8ef2aec3f45a9e83`.
