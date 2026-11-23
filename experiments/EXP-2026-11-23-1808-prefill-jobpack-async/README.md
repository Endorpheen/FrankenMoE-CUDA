# EXP-2026-11-23-1808 — Prefill job packs leave the synchronising copy

Status: **ACCEPTED as a gain, default-on candidate.** Prefill +4.0 %, decode
unchanged, exactness, VRAM and swap unchanged. Base: accepted checkpoint EXP-1805
(`844c5a16`). Candidate: `checkpoints/cand-prefill-jobpack/`,
SHA-256 `e7490685d8a833347cf99c1c663209164c31607ef8e1c2f9c9a9dd44575e940c`.

## Mechanism

`launch_matrix` uploads its job records (`jobs`, three integers per routed
expert row) into `cuda.jobs` before every projection. That pack is 393 KiB for a
4096-token ubatch, and `upload_packet` sends everything above 4096 bytes through
`cuda.upload`, that is `cuMemcpyHtoD_v2`: a synchronous, pageable copy that waits
for the whole legacy stream before it starts and blocks the host until it ends.
Prefill issues 240 such packs per request, measured at 2.7 ms each in
`launch_matrix` (1.289 s for two 4096-token requests).

`upload_jobs_async` fills a four-slot ring of pinned host buffers, queues
`cuMemcpyHtoDAsync_v2` on the same stream and fences each slot with an event, so
the host never blocks and the copy is ordered exactly where the old one was. The
kernels read the same bytes from the same device buffer. Packs of 4096 bytes or
less keep the original path, which is why decode does not move.
`GOOSE_JOBS_ASYNC=0` restores the old copy.

Patch: `source/cuda-driver.patch` (66 lines).

## Measurement

Profile fixed (context 262144, batch/ubatch 4096, KV q4_0, cache 6290 MiB,
request 32768 prompt and 128 predicted tokens). One warm-up and one measured
request per server, one server at a time on 9101. A = `844c5a16`, B = `e7490685`,
alternated A B B A.

| arm | binary | prefill tok/s | decode tok/s | client s | exact |
| --- | --- | ---: | ---: | ---: | --- |
| A1 | 844c5a16 | 2933.9 | 76.338 | 17.955 | yes |
| A2 | 844c5a16 | 2933.9 | 75.880 | 17.997 | yes |
| A3 | 844c5a16 | 2945.5 | 74.510 | 18.074 | yes |
| A4 | 844c5a16 | 2923.3 | 75.462 | 18.073 | yes |
| B1 | adfbca33 (unscoped) | 3066.7 | 75.051 | 17.587 | yes |
| B2 | adfbca33 (unscoped) | 3068.2 | 75.052 | 17.579 | yes |
| B3 | e7490685 (scoped) | 3056.2 | 76.515 | 17.492 | yes |
| B4 | e7490685 (scoped) | 3046.1 | 74.710 | 17.693 | yes |

Medians: prefill A 2933.9 -> B 3051.1 (**+4.00 %**), decode A 75.671 -> B
75.612 (**-0.08 %**, parity). Paired prefill: 2923.3 -> 3046.1 (+4.20 %) and
2933.9 -> 3056.2 (+4.17 %).

The unscoped build (adfbca33, before the 4096-byte guard) also took the small
decode packs through the new ring and lost 1.39 % decode (75.880 -> 75.051); the
guard removed that, and the regression is why the change is scoped by size.

Every run reproduces the 512 token IDs and the text
`f8664331bd30cac8124f48807211ec41b6d0b2b77e301839d8a605e8e2a6e4ce`; peak VRAM
8770 MiB in both arms, VmHWM within 0.03 %, VmSwap 0, no system swap-out.

### Confirmed on the cumulative base

A later block compared the same change on the cumulative baseline — A =
`checkpoints/1806-split-candidate` (`faebfd30`, split only), B =
`checkpoints/cand-prefill-jobpack-split` (`37ac0f1d`, split plus this change) —
both with `GOOSE_DECODE_SPLIT=4` and `GOOSE_DECODE_SPLIT_COUNT=1`, three
alternating pairs:

| arm | prefill tok/s | decode tok/s | prompt_ms | client s | exact |
| --- | ---: | ---: | ---: | ---: | --- |
| A1 | 2875.8 | 73.443 | 11394.6 | 18.446 | yes |
| A2 | 2982.0 | 67.775 | 10988.5 | 18.619 | yes |
| A3 | 2998.5 | 78.421 | 10928.1 | 17.533 | yes |
| B1 | 2941.1 | 72.123 | 11141.5 | 18.316 | yes |
| B2 | 3121.4 | 78.185 | 10497.8 | 17.123 | yes |
| B3 | 3122.4 | 78.225 | 10494.5 | 17.118 | yes |

Medians: prefill 2982.0 -> 3121.4 (**+4.67 %**), `prompt_ms` 10989 -> 10498
(**-4.47 %**); pairs +2.27 %, +4.67 %, +4.13 %; decode medians 73.443 -> 78.185
with the arms interleaved under load 1.16-2.74, so no decode regression. All six
runs exact, peak VRAM 8774 MiB in both arms, VmSwap 0. Best absolute figure of
the block: 3122.4 tok/s.

A follow-up that also queued the small uploads (EXP-2026-11-23-1810) lost 2.95 %
and is rejected, so this change keeps its 4096-byte guard.

The machine was in a slower state during this block than when the 3103.8 tok/s
record was taken (the accepted arm reads 2923-2945 here against 3081-3104 then),
so only the paired delta is quoted. At the recorded baseline rate the same gain
is about 3228 tok/s, still short of the 3500 target.

## Next on the same axis

The instrumented build measures `submit_expert_stage` at 0.051 s for two requests
and `ec_prepare` at 0.22 s, so the staging calls themselves are cheap. What is
left of the host cost is per-copy driver overhead: the prefill trace shows 4811
synchronous `cuMemcpyHtoD_v2` calls taking 5.7 s of host time, most of them below
128 KiB (1.2 ms each), which is the pageable synchronous path. Counting those
calls and their sizes, then routing them through the pinned ring, is the next
step.

## Artefacts

`runs/ab-A1..A4-20261123-*`, `runs/ab-B1..B4-20261123-*`, `source/cuda-driver.patch`,
`checkpoints/cand-prefill-jobpack/{frankenmoe-infer,cuda-driver.o,cuda-driver.c,SHA256SUMS}`.
