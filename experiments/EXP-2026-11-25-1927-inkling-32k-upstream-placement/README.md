# EXP-2026-11-25-1927: Measure a stronger full-context Inkling upstream reference

## Upstream placement and microbatch

The same llama.cpp executable as EXP-1924 was tested with Inkling
context 36864, batch 4096, Q4_0 K/V, Flash Attention requested,
one slot and a 768 MiB automatic fit target. Three startup probes
used the identical launch except for ubatch. At 2048 and 512 the
graph failed to allocate a CUDA prefill buffer. At 256 health
succeeded with all 49 model layers placed on CPU.

| Upstream ubatch | Result | CUDA compute buffer |
| ---: | --- | ---: |
| 256 | Ready, 0/49 layers offloaded | 8612.31 MiB |
| 512 | CUDA OOM, 0/49 layers offloaded | 10483.03 MiB requested |
| 2048 | CUDA OOM, 0/49 layers offloaded | 21779.74 MiB requested |

On the first 4096 IDs of the frozen full request and 64 outputs,
one fresh run per viable candidate produced the following results.
All output IDs and text match across the three runs. Context and
the other launch settings remained the same as the full test.

| Upstream ubatch | Offloaded layers | Prefill, tok/s | Decode, tok/s |
| ---: | ---: | ---: | ---: |
| 64 | 47/49 | 47.99 | 13.05 |
| 128 | 43/49 | 156.45 | 24.81 |
| 192 | 25/49 | 113.03 | 7.08 |

The faster 128 configuration loaded 7675.91 MiB of CUDA model
weights and used 968.43 MiB of CUDA compute buffer. The historical
128 launch in EXP-1924 reported the same layer count but used
1540.15 MiB of CUDA model weights and 6731.45 MiB of CUDA compute
buffer. Layer count alone cannot identify placement when the fit
routine can move individual tensors. This sweep did not isolate
which fit decisions caused the different graph allocation.

## Fresh full comparison

One fresh 32K/512 request per engine on the same RTX 4070 with
identical model, token IDs, sampler, context 36864, batch 4096,
Q4_0 K/V and full prompt recomputation. Use the best screened
upstream profile and the accepted EXP-1923 Goose profile with
the EXP-1925 Q8 descriptor candidate. The engines use different
microbatch sizes and device placement.

| Runtime | Ubatch | Prefill, tok/s | Decode, tok/s | Client, s | Board peak | Peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Goose | 2048 | 192.93 | 13.615 | 207.621 | 10542 MiB | 13.20 GiB |
| llama.cpp | 128 | 138.44 | 21.369 | 260.634 | 11282 MiB | 14.41 GiB |

Both engines produced all 512 identical output IDs and identical
text, with no process swap or system swap-out. Goose prefill
finished 66.86 s sooner and decode took 13.62 s longer. Goose
recorded 40619 expert fills and 175.19 GB prefill upload bytes.
Use this fresh result instead of the weaker historical profile
when setting a decode parity target. The full run has one
observation per engine and compares these usable run profiles.
The 25 tok/s decode and 500 tok/s prefill milestones remain open.

The next comparison should attribute the full decode gap under
these placements. Measure Goose expert residency, transfer bytes
and critical-path overlap. Compare the amount of offloaded expert
weights and work on the CPU in the actual upstream placement.
The data here show a placement difference within upstream; they
do not isolate how much of Goose's decode gap is due to routing,
expert traffic, compute or graph execution.

## Artifacts

The repository changes are evidence only. The frozen Apex
checkpoint manifest passes; no fresh Apex benchmark was run.
See `results.json` and `inputs.sha256` for timings, memory,
placement, arguments and output hashes. Raw receipts are stored in
`build/inkling-exp-baseline/upstream/fit-current-32k-ub128`,
`build/inkling-exp-baseline/upstream/fit-sweep-4096-64` and
`build/inkling-exp-baseline/device-attention/inkling-32k-q8-resident-job`.
The upstream source revision is
`4c92c5bfb7cab395af9fc0fe0939e34ae5083345`.
