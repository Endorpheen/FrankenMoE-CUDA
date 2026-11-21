# Optimized Hybrid performance comparator

## Active reference

The original EXP-1476 release (`e7c9155a`) remains frozen.
Optimized Hybrid currently uses EXP-1597's separate copy of
that release with `--moe-cache-mib 5632`. The executable and
libraries are byte-identical to the frozen artifacts. The
two pairs use one Apex 32K/512 request on RTX 4070 with
262,144 context, Q4_0 K/V, batch/ubatch 4096/4096 and
CPU limits 8/16. The experiment records the exact command,
request and response hashes and verifies the release manifests
before and after. The persistent isolated copy is at
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/launch-vram-exp1597/hybrid-cache-copy`.

| Measure | Frozen EXP-1476, 4,992 MiB | Optimized Hybrid, 5,632 MiB |
|---|---:|---:|
| Prefill median, tok/s | 2,671.366 | 2,668.076 |
| Decode median, steps/s | 62.016 | 65.377 |
| Decode duration on 511 steps, s | 8.240 | 7.816 |
| Peak process / card VRAM, MiB | 8,202 / 10,496 | 8,842 / 11,135 |
| Maximum RSS high-water, KiB | 21,861,340 | 21,861,008 |
| Request process swap and system swap-out growth | 0 | 0 |

Both pairs preserve content, 512 IDs and probabilities.
The active reference's response SHA-256 is
`c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f`.
The gain is higher expert residency. Profile details and all
Goose observations are in the [launch comparison](APEX-LAUNCH-COMPARISON.md).

## Comparison modes

1. **Same controls.** Match the 5,632 MiB configured budget,
   request, model, context, KV, effective CPU limits, physical
   batches, GPU, cache state and measurement conditions. In
   EXP-1597 Goose reports 1,122.585 tok/s prefill and 30.364
   steps/s decode over two requests. Optimized Hybrid requires
   about 9.01 s less decode in the compared medians. This is
   a diagnostic gap between the two observed continuations.
2. **Matched peak VRAM.** Match sampled per-process device
   memory and record card headroom and actual expert-bank
   bytes. The qualified example matches frozen EXP-1476 at
   8,202 MiB with Goose 5,920 at 8,200 MiB. An attempt to
   match the active Optimized Hybrid's 8,842 MiB via a 6,560
   MiB Goose budget is rejected by the live placement guard.
   It reports 6,634,102,784 bytes available after its state
   and model reservations. Equal peak VRAM against the active
   comparator remains open on this 262K profile.
3. **Best verified settings.** Keep runtime-specific settings
   and memory costs explicit. The selected Goose profile is
   5,920 MiB, 1,121.743 prefill and 31.053 decode. Its median
   decode is 8.639 s longer than Optimized Hybrid. The higher
   6,144 MiB setting has a 31.180 steps/s median on two
   observations and costs another 224 MiB peak. Its range
   crosses the selected profile, so the additional memory
   has no demonstrated repeatable speed benefit.

## Keeping the reference current

- Preserve the frozen EXP-1476 files, manifest and existing
  working checkpoint. Run each new hybrid candidate in an
  isolated copy or build with immutable evidence and rollback.
- Promote a new Optimized Hybrid only after repeated,
  same-profile comparisons with the current reference verify
  response quality, speed, allocation, residence and swap.
  Record any resource tradeoff, exact executable and library
  hashes, all rates, request and response digests and traces.
- Refresh CPU/GPU profiles for both active comparators after
  significant changes that can shift the bottleneck. Use phase
  attribution for graph construction, scheduling, launches,
  fusion, CUDA kernels, allocations, transfers and device gaps.
  Attribute differences to concrete mechanisms, implementation,
  configuration or hardware limits before choosing a change.
- Keep hybrid candidates out of the independent Goose runtime.
  Apply measured insights through Goose implementations and
  capability-aware dispatch. The final speed target is better
  than Optimized Hybrid with correct generation and qualified
  memory use.
