# EXP-2026-11-25-1943: Write Inkling prefill outputs directly to the host workspace

**Verdict: ACCEPTED for the isolated Inkling profile.** Direct
pageable output is the default of the candidate. A pinned
destination remains an optional diagnostic mode.

## Mechanism

EXP-1942 identified 96 device-to-host transfers of 36 MiB in a
4096-token microbatch. In the original path each Attention or
MoE graph materialized its output in a separate host arena.
The caller then copied that output into the prefill workspace.

Bind those single-root graph outputs directly to the reusable
Inkling prefill output area before allocating the plan. The
allocator already supports external data storage. The graph
still returns the result to the host before its consumer runs.
The candidate eliminates the separate host result allocation
and copy. It applies only when both resident Inkling graph
paths are enabled and the prefill microbatch has at least
512 rows. One-token decode and the common graph code retain
their existing path. Attention and MoE write the same
workspace in sequence, after the prior consumer finishes.

Use `I2_DIRECT_PREFILL_READBACK=0` to disable direct binding.
Use `I2_DIRECT_PREFILL_READBACK=pinned` only to test a bounded
36 MiB CUDA host allocation. Failed pinned allocation falls
back to direct pageable output.

## Transfer screen

The seeded synchronous CUDA readback benchmark verifies
identical bytes. At 36 MiB, the median pageable transfer is
3.56 ms, direct pinned 1.46 ms, and pinned followed by
`memcpy` 10.86 ms. The last mode is an isolated bounce test.
It is not used by the model candidate. This screen excludes
consumers and graph overhead.

## Paired short workload

RTX 4070, EXP-1927 4096/128 fixture prefix, context 36864,
batch and ubatch 4096, Q4_0 K/V, wide vectors, 736 MiB
activation arena and 6720 MiB expert limit. Six fresh process
launches, first and repeat request per launch, with matching
state checks and full prompt recomputation. The order is
pinned, direct pageable, control, control, direct pageable,
pinned. The same screening binary was used for all arms.

| Paired mean | Original | Direct pageable | Direct pinned |
| --- | ---: | ---: | ---: |
| First prefill, tok/s | 218.44 | 255.12 | 258.52 |
| Repeat prefill, tok/s | 225.04 | 265.70 | 269.71 |
| First decode, tok/s | 17.783 | 18.053 | 18.157 |
| Repeat decode, tok/s | 17.841 | 18.226 | 17.925 |
| Repeat client request, s | 25.541 | 22.602 | 22.492 |

Direct pageable prefill gained 18.07% in the paired mean.
Both pageable pairs improved total request time. Pinned adds
1.51% in repeat prefill relative to direct pageable and loses
1.65% repeat decode throughput in this screen. It also keeps
about 36 MiB more process RSS in the sampled steady state.
Keep ordinary memory as the default. The timed screening
binary defaulted to pinned and explicitly selected pageable
with `I2_DIRECT_PREFILL_READBACK=pageable`. The final binary's
only post-screen source change makes pinned opt-in.

All 8702 fingerprints per launch, both sets of 128 output
IDs and text, and prefill expert upload counters match. The
board peaks for direct pageable match their control arms.
There was no process swap or system swap-out.

## Full workload

One fresh 32K/512 test of the final default compared with
the same-profile saved EXP-1940 reference.

| Full request | EXP-1940 | Direct pageable |
| --- | ---: | ---: |
| Prefill, tok/s | 219.015 | 247.122 |
| Decode, tok/s | 17.657 | 17.633 |
| Prefill, s | 149.615 | 132.598 |
| Client request, s | 179.034 | 162.046 |
| Peak board VRAM, MiB | 11731 | 11730 |
| Peak process RSS, KiB | 14265536 | 14227528 |

Prefill gained 12.83% and saved 17.017 s. The single full
decode observation is 40 ms slower. The complete request
saves 16.987 s. All 512 output IDs and text match. Prefill
expert traffic remains 20760 fills and 89.54 GB. Process swap
and system swap-out stayed at zero. The full comparison has
one run per binary.

The frozen Apex checkpoint manifest passed. Production
launchers and the operational Apex binary were left in place.
This experiment did not measure Apex throughput.

## Reproduction

Apply the CUDA header patch to the EXP-1940 input and the host
patches to EXP-1937. Use the provided build helper to link the
EXP-1940 archive with the modified Inkling objects. See
`inputs.sha256` for both executable checkpoints and
`results.json` for exact settings and per-run correctness.
The seeded copy screen is `source/bench-readback.py`.
