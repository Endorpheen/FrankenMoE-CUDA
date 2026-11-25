# EXP-2026-11-25-2017 — Exact small-batch Bonsai GDN decay

**Accepted in the experimental Bonsai HTTP line.** Process one token
with 256 threads per head. The CTA stages the 5120 F32 products for
each of alpha and beta in shared memory. The original 32 lane-local
sums and butterfly order are preserved by separate warps. The
softplus/sigmoid epilogue is unchanged. For 2–31 tokens use the
existing EXP-1998 warp-pack kernel. The proven batch reuse remains
active at 32 or more tokens. No new global workspace is allocated.

## Attribution and correctness

Seven component cases on real layer-0 and layer-42 weights pass an
exact byte comparison for both outputs. At one token the base
takes 27.63 us, EXP-1998 warp pack 12.64–13.26 us, and the selected
256-thread stage 9.66–9.73 us. Warp pack wins for the sampled 3/17/31
token cases. The selected stage uses 30 registers, 40,960 shared
bytes and no register spills.

The matched 32768/16 CUDA traces report 720 decode decay dispatches
in each arm. Their sum falls **31.802 → 4.668 ms** over 15 continuation
steps, or **1.809 ms saved per step**. Profiled decode wall time falls
488.775 → 461.929 ms and all 16 output IDs match PrismML. This is
direct evidence for the kernel contribution. Traced throughputs
are outside the unprofiled performance pairs.
[CUDA summary](data/trace-summary.json) and [component data](data/component.json).

## Unprofiled HTTP results

Same binary, CUDA images and EXP-2016 driver loader on RTX 4070.
Context 36864, batch/ubatch 4096/1024, Q4_0 K/V, affinity 0f0f,
greedy, EOG masked, MTP and prompt reuse off. A uses the original
decay kernel for small batches. B runs the new defaults. The
4110/64 profile uses a trailing multi-token microbatch in addition
to one-token decode.

| Workload | Prefill A → B, tok/s | Decode A → B, tok/s |
| --- | ---: | ---: |
| 4110/64, two pairs, aggregate | 784.15 → 801.55 | 35.85 → 38.60 |
| 32768/512, A–B | 600.46 → 611.03 | 30.916 → 30.602 |
| 32768/512, B–A | 606.23 → 621.01 | 30.787 → 32.552 |
| 32768/512, aggregate | 603.33 → 615.98 | 30.851 → 31.547 |

Both short pairs improve decode, +7.68% from aggregate times. Long
decode changes -1.01% and +5.73%, with +2.25% from aggregate times.
Whole-request client time improves in both long pairs (71.121 →
70.349 s and 70.672 → 68.489 s). The experiment affects only the
short tail of long prefill. The much larger observed prefill shifts
are not attributed to this kernel. Board-clock samples were captured
in the reverse pair and do not establish the reason for run variation.

All 512 IDs and UTF-8 text SHA-256 in each scored long arm match
PrismML. All 64 short outputs and text agree across the short arms.
Peak VRAM is 8932 MiB in every scored arm. Long VmHWM is 7,457,232–
7,457,444 KiB for A and 7,457,328–7,457,540 KiB for B. Process
and system swap-out stay zero. Scope is Bonsai only. The existing
Apex and Inkling release paths remain unchanged.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2017-bonsai-decode-decay
dev=build/bonsai-decode-decay-repro
out=build/bonsai-decode-decay-http-repro
bash "$ex/source/build.sh" "$dev"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$dev/decay.cubin"
bash "$ex/source/build-runtime.sh" "$out" "$dev"
```

Use the EXP-2016 server options with the new executable and
`BONSAI_CUDA_IMAGE="$out/decay.cubin"`. For A set
`BONSAI_DECAY_SMALL=0`. B leaves it unset. Post EXP-1977 HTTP
`/completion` at 32768/512, or the first 4110 input IDs and 64
output tokens. Use 32768/16 with CUDA-only Nsight profiling to
reproduce attribution. [Scored responses and memory](data/results.json).
[Artifact hashes](data/SHA256SUMS).
