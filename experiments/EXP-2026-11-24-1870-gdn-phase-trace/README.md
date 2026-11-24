# EXP-2026-11-24-1870 — Trace the GDN experiment phase by phase

**Diagnostic only.** EXP-1869 remains rejected under its unprofiled
A-B-B-A outcomes. This pair links the same EXP-1869 archive with
an NVTX wrapper around generation evaluation. The EXP-1869 toggle
is the only intentional runtime difference between the two arms.

RTX 4070, Apex 32K/512, context 262144, expert cache 6290 MiB,
batch and ubatch 4096, Q4_0 KV. One warm-up and one profiled request
per fresh process. Nsight Systems 2024.5.1 captures the second request.
The wrapper separates prefill from the 511 decode evaluations.
A sets `GOOSE_GDN_V32_FULL=0`; B leaves it unset and logged the
full-block entry. All four replies matched every one of the 512 token
IDs and exact text. Post-request properties matched byte-for-byte.
Runtime swap was zero, but system swap-out rose by 21914 pages in A
and 3802 in B. This adds a confound to the profiled comparison.

| NVTX or CUDA measure | Control ms | GDN candidate ms |
| --- | ---: | ---: |
| Prefill range | 9862.39 | 9827.22 |
| GDN V32 kernels, 240 launches | 601.77 | 571.75 |
| Decode range | 6809.28 | 6786.51 |
| Decode kernel interval union | 5155.78 | 5157.79 |
| Decode memcpy interval union | 458.61 | 455.28 |
| Decode visible GPU activity union | 5364.46 | 5363.13 |
| Decode range outside observed GPU intervals | 1444.82 | 1423.38 |

Decode had 875343 kernel intervals and 170022 transfers in each arm.
Counts match by kernel name and copy kind. Recorded transfer bytes
match by kind. Median graphics and memory clocks were 2835 and
10251 MHz on both arms. Graphics clock ranges at 250 ms sampling were
2805–2835 MHz for A and 2835 MHz for B. Median temperature was 61 °C
versus 58 °C. These samples cannot rule out shorter transients or
host timing changes.

The API reports decode at 16845.76 and 16562.05 ms because the capture
stop and teardown affect wall time. The marked decode intervals
exclude that stop path. The observed GPU gaps can include activity
the trace does not represent. The pair measures about 30 ms less GDN
kernel time and does not reproduce the unprofiled decode decrease.
The cause of the unprofiled decrease remains unproven.

The control trace also localizes 2480 synchronous H2D transfers of
12 KiB during prefill. They occupy 3.21 ms as GPU transfers and
5031.33 ms inside CUDA driver calls. The latter includes waiting
for previous work and is not a direct speedup budget.

## Reproduction

Build [mark-phases.c](mark-phases.c) with GCC 13, EXP-1856's headers
and CUDA 12.6 NVTX headers. Relink the original infer-main and
EXP-1869 archive with `-Wl,--wrap=fg_q35_generation_eval -lnvToolsExt`
and the CUDA library path. Use the checkpoint env and argv. Capture
each second request with the following:

```sh
NSYS_NVTX_PROFILER_REGISTER_ONLY=0 nsys profile --trace=cuda,nvtx --sample=none --cpuctxsw=none \
  --cuda-graph-trace=node --capture-range=nvtx --nvtx-capture=apex-request@FrankenMoE \
  --capture-range-end=stop --kill=none --force-overwrite=true -o "$run/prof" "$bin" "${args[@]}"
nsys export --type sqlite --output "$run/prof.sqlite" "$run/prof.nsys-rep"
python3 analyze.py "$control_run" "$candidate_run" "$exact_reference_run"
```

Sample nvidia-smi fields timestamp, clocks.gr, clocks.sm, clocks.mem,
temperature.gpu, power.draw, utilization.gpu and memory.used every
250 ms into `gpu-2-samples.csv` during the second request.
[results.json](results.json) records run names, trace hashes and all
phase data. EXP-1863 remains the accepted baseline.
