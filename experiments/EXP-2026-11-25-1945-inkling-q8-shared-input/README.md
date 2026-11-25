# EXP-2026-11-25-1945: Screen shared-memory Q8 input reuse for Inkling

**Verdict: REJECTED for the tested kernels.** A synthetic screen
shows slower device time across the tested Q8_0 shapes. The
candidate was not enabled in the model dispatcher. EXP-1943 and
the EXP-1944 8192/256 test profile remain the working baselines.

## Hypothesis and implementation

The EXP-1942 late-prefill profile attributes 4362.6 ms of the
last 4096-token microbatch to dense Q8 projections. Existing
eight-input tiles reuse a decoded Q8 weight across eight jobs.
Each warp reads the corresponding FP32 activations for its
output row. Share those input loads across 4, 8 or 16 output
warps with a CTA-local FP32 tile. Preserve the row count,
per-output FP32 FMA recurrence, and warp reduction order. All
outputs are compared byte-for-byte with the original kernel.

The 256-channel tile requires 8192 shared bytes per CTA.
The 768-channel tile requires 24576 bytes and runs fewer
barriers for widths 2304 and 3072. Both versions constrain the
synthetic inputs to exact tile multiples. The benchmark uses
eight-input jobs and 3072 output rows. No candidate is wired
into runtime dispatch.

## Device baselines

RTX 4070, SM89, seeded Q8_0 blocks and FP32 inputs. Each shape
has matching output hashes. CUDA event timings alternate variant
order over four samples. The following figures are microseconds
per kernel. All measurements are for resident synthetic inputs.

| K width | Input jobs | Original | Best shared 256 | Best shared 768 |
| ---: | ---: | ---: | ---: | ---: |
| 2304 | 16 | 92.22 | 111.31 | 109.85 |
| 2304 | 128 | 665.90 | 735.14 | 737.91 |
| 2304 | 512 | 2686.26 | 2980.13 | 2942.79 |
| 3072 | 16 | 110.22 | 131.38 | 131.05 |
| 3072 | 128 | 903.27 | 987.98 | 948.76 |
| 3072 | 512 | 3551.05 | 3833.15 | 3866.42 |

Increasing row warps helped the shared-memory variants but did
not reach the original. Larger shared tiles did not eliminate
the gap. The results do not isolate synchronization, occupancy or
memory-bank behavior. They rule out these exact staged layouts as
an end-to-end experiment candidate. A different mechanism should
get a device win before entering the 8192/256 model gate.

A separate short attribution check measured the existing CUDA
context push/pop pair at 0.023–0.024 microseconds on a single
thread without concurrency or graphs. This does not measure
lock contention. Avoid changing context ownership solely to
remove the measured push/pop pair.

## Reproduction

Apply `source/matrix.cl.patch` to the EXP-1936 device sources
and use the build script with the local EXP-1943 archive and
headers. Run `bench-shared.py` in the build directory.
The original and five shared entries are selected only by
the benchmark. See the JSON files and compiler receipt for
all variants and samples. `bench-context.c` uses the CUDA
driver directly and can be built with GCC 13 and the CUDA
header. The frozen Apex checksum manifest passed. No model
runs or Apex performance checks were performed.
