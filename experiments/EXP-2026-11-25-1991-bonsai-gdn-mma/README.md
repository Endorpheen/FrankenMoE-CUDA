# EXP-2026-11-25-1991 — Use the frozen Goose GDN V32 path for Bonsai prefill

**PROVEN for short greedy IDs and prefill gain.** Bonsai now dispatches
the previously frozen Goose V32 GDN MMA kernel when its input batch
contains at least 64 tokens. Its geometry has 16 Q/K heads and 48
value heads, width 128, scalar gate, and one final state. The graph,
the weights, the existing CUDA images, and state allocation do not
change. The dispatcher keeps the scalar batch and decode routes for
short tails. The shared Apex and Inkling paths are unchanged.

## Component check

RTX 4070, identical packet layout and strides to the Bonsai graph,
normalized seeded Q/K, synthetic V, decay and beta, nonzero starting
state. Scalar GDN, full-width MMA and V32 are compared on the same
CUDA context. Outputs, final state, next scalar decode output, and next
state are all compared. Component elapsed time is median of four
repeated CUDA-event timings.

| Input tokens | Scalar, ms | Full MMA, ms | V32, ms |
| --- | ---: | ---: | ---: |
| 65 | 1.568 | 0.129 | 0.091 |
| 257 | 6.142 | 0.373 | 0.264 |
| 1024 | 24.397 | 1.412 | 0.980 |

All modes pass the relative L2 limit of 0.001. Max observed relative
L2 is below 0.0000069 across outputs and states, including the transition
to scalar decode. The V32 path is about 24.9x faster on the 1024-token
kernel invocation. V32 uses 38,016 B of dynamic shared memory. No
additional global device workspace is allocated.

## Full short gate

4097/16, context 36864, ubatch 1024, Q4_0 K/V, same raw input IDs,
greedy, EOG masked, MTP disabled. A–B–B–A with EXP-1990 MMQ and
EXP-1988 Flash active in both modes. Prefill saves 4.289 s and 4.131 s
in the two pairs. All 16 IDs match the current control and the same-input
PrismML reference from EXP-1985. The selected logits can differ, with
a maximum absolute difference of 0.6563 on this fixture. The accuracy
claim is exact IDs on this fixture, with numerical agreement on the
component checks. It does not claim bitwise full logits.

Matched pairs at 273/8 (ubatch 128) and 1025/16 (ubatch 1024) also
preserve all native IDs. The 273-token fixture exercises a short final
microbatch that falls back to scalar GDN. One 273/8 test without the env
override verified the V32 default and exact replay of the explicit V32
candidate. Global CUDA working storage remains 1,720,400,640 B at
ubatch 1024. Peak system RAM and device memory were not sampled here.

## Updated loss map

The candidate 4097/16 profile shows **179.30 ms for batch GDN**,
versus 4455.85 ms in EXP-1990. The overall prefill GPU span is
8.601 s. PQ2 MMQ accounts for 7.116 s, followed by F32 decay/beta
projections at 0.362 s, async Flash tiles at 0.201 s, and GDN at
0.179 s. These short traces are diagnostic. Whole-model long
performance and broader correctness remain pending. The next prefill
candidate should reduce redundant PQ2 tile staging and unpacking.
The user-facing goals of 1858.46 prefill, 81.55 decode and cross-model
regression qualification on 32K/512 remain open.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1991-bonsai-gdn-mma
bash "$exp/source/build.sh" build/bonsai-gdn-mma-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-gdn.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-gdn-mma-repro/frozen-goose.ptx
```

Use the EXP-1990 invocation with `bonsai-gdn-mma`. Set
`BONSAI_GDN_V32=0` for the control. V32 prefill is enabled by default
in this Bonsai experimental build. Use the Nsight commands from
EXP-1990 to profile the new executable. Raw trace is retained at
`build/bonsai-gdn-mma-1991-runtime/profile.nsys-rep`.

[Component checks](component.json). [Short requests](results.json).
[Updated prefill profile](profile.json).
