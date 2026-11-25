# EXP-2026-11-25-1934: Retain Inkling Attention and MoE decode plans

**Verdict: ACCEPTED for the isolated Inkling checkpoint.** Both short
pairs and one full-workload comparison show a decode gain. GPU kernels
and expert routing remain those of EXP-1932.

## Mechanism

The EXP-1933 diagnostic measured 3.08 ms per token in graph plan
allocation and about 0.40 ms in context allocation and graph creation.
The host previously rebuilt the Attention and MoE plan for each
of the 48 logical visits on every token. Retain one plan per visit
for each stage, owned by the Inkling decoder. The first decode token
creates the plans and their arenas. Subsequent evaluations rebind the
current input and message tensors, update the attention position
and MoE epilogue position, then compute with the existing planner.

The plan keeps the fixed topology, weight bindings, intermediate
layout and lifetimes. Every execution still computes fresh activations,
attention and expert IDs. The attention metadata is stored in
the plan entry, so its lifetime covers every execution. MoE and
Attention cache entries have separate arenas. Request reset retains
the topology. Closing the decoder releases all plans and contexts.
Prefill, aggregate compute and Apex dispatch do not use these
entries. Use `I2_RETAIN_DECODE_GRAPHS=0` to rebuild plans for the
control with the same executable.

## Paired 2048/256 screen

EXP-1917 fixture, RTX 4070, context 36864, batch 4096, ubatch
2048, Q4_0 K/V, wide vectors, a 576 MiB activation arena, and
6720 MiB expert limit. A–B–B–A, one first request and one repeat
per fresh launch, full prompt recomputation and matched hashing.

| Paired mean | Rebuild | Retain |
| --- | ---: | ---: |
| First prefill, tok/s | 201.77 | 201.42 |
| Repeat prefill, tok/s | 215.37 | 214.55 |
| First decode, tok/s | 15.524 | 16.920 |
| Repeat decode, tok/s | 15.455 | 16.854 |
| Repeat decode, ms | 16499.7 | 15130.2 |
| Repeat client request, s | 26.226 | 24.897 |

Repeat decode gained 9.05%. The two pairs saved 1378 and 1361 ms
of decode. All 5118 state fingerprints per launch, both sets of
256 output tokens and text, and prefill expert fills and bytes
matched. Peak process RSS rose 4460 KiB across arm maxima.
Board VRAM was within the session-to-session sampling range.
Process swap and system swap-out were zero.

## Full workload

One fresh 32K/512 request on the candidate compared with the
corresponding saved EXP-1932 run on the same hardware and profile.
Internal fingerprinting was disabled for both full timing runs.
Both generated all 512 identical IDs and identical text.

| Full request | EXP-1932 | Retained plans |
| --- | ---: | ---: |
| Prefill, tok/s | 192.792 | 191.925 |
| Decode, tok/s | 15.285 | 16.608 |
| Decode, s | 33.432 | 30.769 |
| Client request, s | 203.893 | 201.976 |
| Peak board VRAM, MiB | 11275 | 11277 |
| Peak process RSS, KiB | 13840624 | 13841212 |

Full decode saved 2.663 s, or 8.66% in throughput, at the cost of
0.45% lower measured prefill. Prefill expert traffic matched at
40619 fills and 175.19 GB. No process swap or system swap-out was
recorded. The remaining decode gap versus the fresh llama.cpp
reference of 21.369 tok/s is 6.86 seconds over 511 intervals.
The full difference comes from one run per binary. The repeat
paired evidence is from the short screen.

The frozen Apex checkpoint checksum manifest passed. No
combined-runtime Apex performance check ran. The production
runtime and deployment profiles were not changed.

## Reproduction

Apply `source/inkling-backend.c.patch` with `patch -p1` to
the EXP-1921 host inputs in the local sibling build layout.
Use `source/build-candidate.sh` to compile the Inkling host
object with GCC 13 and relink the EXP-1932 archive and numerical
object. All device images remain the same. See `inputs.sha256`
and `results.json` for the binary, request hashes, launch
arguments and individual timings.
