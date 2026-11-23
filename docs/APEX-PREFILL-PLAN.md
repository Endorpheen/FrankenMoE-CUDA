# Apex prefill: where the 3500 tok/s target starts from

Baseline and checkpoint: `docs/APEX-CHECKPOINT.md`. Profile is fixed (context
262144, batch and ubatch 4096, KV q4_0, cache 6290 MiB); a prefill change may not
alter it, may not cost decode, VRAM or exactness.

## The arithmetic

32768 prompt tokens at 3103.8 tok/s is 10557 ms of `prompt_ms`. 3500 tok/s is
9362 ms: 1.2 s must leave the prefill window, 11.3 % of it. The request is
processed as eight ubatches of 4096 tokens, about 1320 ms each, so the target is
about 1170 ms per ubatch.

## What is already established, and what is not

Established by the source of the measured build (`cand/split/src/cuda-driver.c`):

- the capture and replay machinery is decode-only: `decode_capture`,
  `decode_launch`, `decode_capture_submit` and the region caches are entered only
  when decode state is active, and `fg143_cpu_prefill()` gates the decode-only
  fast paths. Prefill executes its kernels directly, one launch per node;
- prefill has its own expert path: `expert_prefill_staging`,
  `prefill_upload_misses`, `submit_pageable_expert` with a host ring, and
  `expert_prefill_layout` for the temporary workspace;
- the accepted profile already sets the MMQ switches
  (`GOOSE_MMQ_Q4_LARGE_TILE=128x256`, `_PREPARE`, `_PIPELINE`,
  `_ROUTED_M128`, `_ROUTED_DOWN_M128`, `GOOSE_MMQ_Q6_*`,
  `GOOSE_MMQ_SWIGLU_PACK`);
- a prefill CUDA graph has never been built: the card that would have retained
  one, EXP-2026-10-31-126, says `NOT_RUN` in its own text.

Not established: the prefill time split for this runtime. The only saved profile
of this build (`runs/nsys-pf2-20261123-175657/prof.sqlite`) begins at the first
decode step: its first 32-byte read-back is at 0.00 s of a 7.20 s kernel span, so
it contains decode only. The September prefill profile (EXP-043) was taken on the
llama.cpp-era substrate with a 395-token request and cannot be transferred.

## Hypotheses, in the order they should be measured

H1. **Prefill host submission.** If prefill issues one launch per node, a 4096-token
ubatch issues thousands of launches while the GPU waits. Measure: launch count and
GPU idle union inside the prefill window. If idle exceeds 3 %, the lever is a
prefill region graph (the decode analogue already bought 1.0-1.6 % at 40 launches
per step). Cost: one profile, then a capture implementation.

H2. **Prefill expert staging.** This one is already measured from the server log of
a recorded run, without a new profile: `Goose CUDA prefill temporary: 960
projections, 59856 resident planes, 55334240256 uploaded bytes, 627 prefetched, 0
discarded` and `prefill compact: 13 full-buffer projections, 13 prefetch skips for
capacity`. Two requests (warmup and measured) upload 55.3 GB, that is about 27.7 GB
per request; at the roughly 20 GB/s this path sustains that is 1.4 s of transfer
inside a 10.6 s prefill, and 13 projections were skipped because the 210 MiB
workspace was full. Lever: admit more of the working set, or overlap the gather of
one ubatch with the MMQ of the previous one. Caution: EXP-1692/1693/1694 already
tried larger temporary planes and packed slices on the EXP-1691 base and lost
(2996-3009 tok/s against 3009), so the change must be an overlap, not only a
bigger buffer.

H3. **Prefill MMQ shape.** The ubatch is fixed at 4096 rows, and the tile switches
were tuned for it, but the routed and down projections run at a different row
count. Measure: per-kernel time for the eight ubatches. Lever: an env-gated tile
variant, no profile change. Cost: an env sweep, one arm per variant.

## Measurement protocol

One server at a time on 127.0.0.1:9101, one warmup and one measured request, arms
alternated ABBA, exactness checked on the 512 IDs and the text
`f8664331bd30…`, peak VRAM, VmHWM, VmSwap and `pswpout` recorded per arm, and the
A/B accepted only if every pair is clean of system swap-out (the EXP-1806 lesson).

## Blocker

Port 9101 is held by a running server started 2026-11-23 18:40
(`frankenmoe-server`, pid 1182854, `--expert-cache-mib 6400`, release build in
`/home/end0/AI/claude-apex-100/release/linux/`), and it is serving a live chat.
Every measurement above needs that port, so it may not be started while that
server runs, and it must not be stopped without the owner's word.

## Update, 2026-11-23 late: every axis in this plan is measured and closed

- H1 host submission: closed, four candidates rejected (EXP-1810..1813).
- H2 prefill staging volume: the eviction policy is within 2.3 % of the Belady
  optimum at the same capacity (EXP-1816), so the volume cannot be reduced by a
  policy.
- H3 prefill MMQ shape: every switch variant is slower than the accepted set
  (EXP-1815).
- Cache capacity: fixed by the profile, 6510 MiB measured slower, 7500 refused.
- Prediction-based prefetch: 0 of 96 162 copies predictable (EXP-1802).

Prefill therefore stands at 3122.4 tok/s in the current block conditions, with
the dependency chain of expert transfers as the measured bound of this
configuration. The remaining levers (more VRAM, or a model that fits the cache)
change the accepted profile.
