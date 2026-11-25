# EXP-2026-11-25-1968: Attribute the current Inkling 32K prefill cost

**Verdict: diagnostic.** Prioritize the dense Q8 attention projections.
The accepted EXP-1967 performance baseline stays in place.

Build the EXP-1966 host with EXP-1942 phase instrumentation and
capture the final 4096-token microbatch via NVTX on RTX 4070.
Use the full 32K/512 fixture and the combined decode settings.
The 6720 MiB expert cache failed at a 144 MiB MoE output allocation,
both with Nsight (first batch) and without it (second batch), at
2015 and 2031 MiB board usage before launch. The successful diagnostic
uses 6464 MiB expert cache with the same 736 MiB activation arena,
context 36864, Q4_0 KV and batch/ubatch 4096. Do not substitute the
diagnostic cache settings for the performance reference.

## Prefill phase budget

Scope times include CPU submission and waiting within the operation.
The phase timers across the eight microbatches report:

| Phase | Total, s | Last batch, s |
| --- | ---: | ---: |
| Attention graph | 62.815 | 8.865 |
| MoE graph, including final host copy | 34.895 | 4.381 |
| Input preparation | 7.837 | 0.981 |
| Residual and FFN preparation | 4.146 | 0.538 |
| Post-attention convolution | 1.916 | 0.255 |
| Diagnostic request prefill total | 126.796 | — |

The initial host-only batch at 6720 MiB took 14.729 s, including
6.929 s for Attention and 4.672 s for MoE. The matching trace setup
at 6464 MiB took 14.724 s on the first batch. Timing from the failed
launch describes only that completed batch. The final NVTX range
was 16.432 s. The surrounding host wall timer for that batch reads
20.380 s, including overhead outside the captured range.

| Exclusive CUDA activity in final NVTX range | Seconds |
| --- | ---: |
| Kernels only | 9.748 |
| No recorded GPU activity | 3.931 |
| H2D only | 2.140 |
| D2H only | 0.384 |
| Concurrent activity classes | 0.229 |
| D2D only | 0.001 |

The summed kernel families are Q8_0 projections 4.426 s, Flash
Attention prefill 2.727 s, and grouped IQ4_XS/IQ4_NL MoE 1.854 s.
They are included in the timeline above. Q8_0 shape attribution from
the launch grid and the four-row/eight-input tiling is:

| Attention output rows | Launches | Summed time, s |
| ---: | ---: | ---: |
| 3072 | 96 | 2.108 |
| 2304 | 192 | 1.280 |
| 1024 | 96 | 0.701 |

Together the three shapes account for 4.089 s of final-batch Q8 time.
Each launch covers 4096 inputs. The width and source projection
must be taken from the model graph when generating reusable tests.
The prior Q8 shared-input layouts in EXP-1945 were slower. The
archived dense Q8 Tensor Core candidate also failed bitwise and
run-to-run state equivalence. Use the three dominant shapes for
a new exact-order kernel probe before seeking a model speedup.

The trace has 96 readbacks and 448 uploads of 36 MiB activation rows.
D2H-only time is much smaller than in EXP-1942, although the captures
use different implementations and expert limits. The remaining CPU
and PCIe boundaries matter, but removing all 0.384 s of measured
D2H-only time in this batch would not reach the 500 tok/s target.
Do not add the host scopes and the CUDA categories together.

## Correctness and reproduction

The diagnostic finished all 512 IDs and text exactly as EXP-1967.
It observed 258.43 prefill / 21.00 decode tok/s with the profiling
setup, 11517 MiB peak board VRAM and 14770972 KiB peak process RSS.
No system swap-out or process swap. No runtime source, Apex path
or production profile changed.

Build with `bash source/build.sh /absolute/new/scratch`. Use
`ASSERT_SOURCE_DIR` set to the absolute original
`build/inkling-apex-gate/prefill-profile-1968` directory to match
GCC assertion paths. The rebuilt binary matched byte for byte.
Use the argv/env and Nsight options in `results.json`, then run
the baseline 32K/512 request. The CUDA capture is triggered by
the final-microbatch NVTX range. Export it to SQLite and use the
EXP-1942 `analyze-prefill.py` and the recorded `q8-shapes.sql`.
Raw data are under `build/inkling-apex-gate/prefill-profile-1968`.
