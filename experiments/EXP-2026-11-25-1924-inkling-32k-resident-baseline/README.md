# EXP-2026-11-25-1924: Establish an Inkling 32K/512 baseline with resident MoE

The EXP-1923 profile completed the frozen Inkling 32768-input,
512-output request on RTX 4070 without swap. This is one full
Goose request from a fresh process. The comparison uses two
previously recorded llama.cpp requests with the same model,
input IDs, sampling options, context 36864, batch 4096 and
Q4_0 K/V. Each request recomputed the full prompt. Goose uses
ubatch 2048 with the 576 MiB activation arena and wide vectors.
The saved llama.cpp profile uses ubatch 128 with automatic
placement and a 768 MiB fit target. The comparison applies to
these recorded profiles. The optimized 2048/256 llama.cpp
result in EXP-1917 uses a different context and workload.

| Runtime and request | Prefill, tok/s | Decode, tok/s | Client request, s |
| --- | ---: | ---: | ---: |
| Goose, fresh | 192.75 | 13.468 | 208.454 |
| llama.cpp, fresh | 82.05 | 11.930 | 442.185 |
| llama.cpp, repeat | 82.96 | 12.007 | 437.551 |

Goose prefill took 170006.7 ms and decode took 37942.0 ms across
511 inter-token intervals. Board VRAM peaked at 10558 MiB and
process RSS peaked at 13842008 KiB (13.20 GiB). The saved upstream
peak board VRAM was 8966 MiB and repeat process RSS was 15132428 KiB
(14.43 GiB). Both runs report zero process swap and system
swap-out. Peak VRAM is sampled board-wide.

All 512 output IDs and the response text match both original
upstream response files. `results.json` records hashes of the
output rather than the prompt or response contents. This is
bitwise agreement on the recorded generation. Internal state
hashing was disabled for the full performance measurement.

## Next measurement priorities

* Confirm the full-context profile against a newly measured
  llama.cpp sweep near its usable microbatch and placement limits.
* Profile Goose's later microbatches. The full fresh prompt used
  40619 expert fills and 175.19 GB of expert uploads. At the
  previously measured 18.49 GB/s, this volume corresponds to
  about 9.5 seconds of exclusive transfer service under ideal
  conditions. This is a transfer scale estimate. It does not
  explain the entire measured 170 seconds or account for overlap.
* Recheck the remaining Attention and MoE activation round trips
  and windowed attention as position grows. EXP-1923 removed the
  large product fallback at ubatch 2048 on the short fixture.

This establishes a full-workload starting point. The 500 prefill
and 25 decode milestones remain open, as does repeated
full-workload measurement. No runtime sources or deployment
profiles changed. The frozen Apex checksum manifest passed.
An Apex performance regression test remains open before a
combined-runtime promotion.

## Artifacts

Use the EXP-1921 executable and the request hash in `inputs.sha256`.
Exact launch arguments, timings and memory are in `results.json`.
The Goose raw receipts are under
`build/inkling-exp-baseline/device-attention/inkling-32k-resident-baseline/ab/resident-cold`.
Saved upstream revision `4c92c5bfb7cab395af9fc0fe0939e34ae5083345`
and raw receipts are under `build/inkling-exp-baseline/upstream/32k-fit`.
