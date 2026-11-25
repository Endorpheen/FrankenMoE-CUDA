# EXP-2026-11-25-1969: Vector-pack the Inkling Q8 prefill inputs

**Verdict: confirmed Inkling prefill improvement.**
Keep as an opt-in candidate. The Apex performance gate remains open,
so the accepted performance profile stays on EXP-1967.

Interleave the existing eight-input tile, with two planes of four
FP32 values per channel. Each lane loads two aligned four-value
vectors instead of eight scalar values drawn from separate input
rows. Q8 decode, round-to-nearest FP32 FMA order and the five warp
reductions are preserved. The CUDA packer consumes the existing
job offsets, without assuming physically contiguous input rows.
The driver reuses the existing MoE packing scratch on the same CUDA
stream. On scratch allocation exhaustion the original entry is used.

Use this only for two-word Q8 jobs on the measured Inkling shapes,
at 512–4096 inputs divisible by eight. Decode and Apex retain their
existing paths. The bit-exact 4096-input device probe covers 2304
and 3072 input channels with 1024, 2304 or 3072 output rows. The
selected eight-warp projections including packing improve 29.7–39.3%
over the original device timings, with 40 registers and zero spills.
The short 512/16 model check activates the new entry and matches
all 543 state fingerprints, 16 output IDs, text and properties.

## Inkling 32K/512

Same binary and EXP-1967 settings. First request per process,
A–B–B–A. Toggle `I2_Q8_PACKED_INPUT` only. Context 36864,
batch/ubatch 4096, Q4_0 KV and expert limit 6720 MiB.

| Arm | Prefill tok/s | Decode tok/s | Prefill s | Full request s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 268.659 | 21.941 | 121.969 | 145.575 |
| B1 | 296.460 | 22.110 | 110.531 | 133.946 |
| B2 | 296.028 | 21.959 | 110.692 | 134.281 |
| A2 | 266.842 | 21.875 | 122.799 | 146.471 |

Mean prefill is **267.751 → 296.244 tok/s (+10.64%)**. The
pairs save 11.438 and 12.107 s of prefill, respectively.
Mean decode is 21.908 → 22.035 tok/s. Mean full response
is 146.023 → 134.113 s. All 512 IDs, text and properties including
expert traffic match across arms. Maximum board peak stays 11735 MiB
and maximum process RSS does not increase. No swap-out or process swap.

## Apex gate

Use the exact pinned Apex profile and second request after full
warm-up. No new Q8 dispatch is logged, and all 512 output IDs, text
and properties match EXP-1966 on every arm. The second-attempt
candidate/control pair runs at 3433.753/80.963 and 3422.236/80.299
prefill/decode tok/s with the same 10734 MiB board peak. However,
the candidate retained 39108 KiB of VmSwap and the matched control
had 469 system swap-out pages during the second request. The first
pair also had swap and is excluded. These observations do not
qualify Apex performance or a production rollout.

## Reproduction

Build with `bash source/build.sh /absolute/new/scratch`. Set
`ASSERT_SOURCE_DIR` to the absolute original
`build/inkling-apex-gate/q8-packed-input-1969` directory to copy
GCC assertion paths. The normalized rebuild matches the exact binary.
Use `python3 /absolute/new/scratch/bench-packed.py` for the
standalone device comparison and `results.json` for the model argv,
env and timings. The main Inkling performance fixture remains 32K/512.
The short diagnostic truncates its input to 512 IDs and requests
16 output IDs. Raw evidence is under
`build/inkling-apex-gate/q8-packed-input-1969`.
