# EXP-2026-11-24-1860 — Validate the causal Flash candidate across prefill shapes

**INCONCLUSIVE for Apex throughput.** This diagnostic uses the same archived
control and fully visible tile cubins as
[EXP-1859](../EXP-2026-11-24-1859-flash-interior/README.md). It makes no new
runtime change. The measurement covers all eight 4096-token query ubatches
of the Apex 32K prefill shape on the RTX 4070. K and V are synthetic staged
FP16 arrays and Q is synthetic FP32. Each median uses six three-launch
CUDA-event blocks per arm, with the arm order alternating.

| KV tokens | Prefix | Control ms | Full tile ms | Time reduction |
| ---: | ---: | ---: | ---: | ---: |
| 4096 | 1 | 3.448 | 2.670 | 22.57% |
| 8192 | 4097 | 9.060 | 6.716 | 25.87% |
| 12288 | 8193 | 13.981 | 10.446 | 25.29% |
| 16384 | 12289 | 19.605 | 14.569 | 25.69% |
| 20480 | 16385 | 25.063 | 18.801 | 24.98% |
| 24576 | 20481 | 30.390 | 22.922 | 24.57% |
| 28672 | 24577 | 36.005 | 27.414 | 23.86% |
| 32768 | 28673 | 41.612 | 31.505 | 24.29% |

All 134,217,728 FP32 output words across the eight shapes matched
bit-for-bit. [Raw samples](sweep.log) and [structured results](results.json)
include all block timings.

Applying each measured synthetic time ratio to its corresponding
[traced control group](../EXP-2026-11-24-1858-prefill-consumer-fence/README.md)
accounts for about 421 ms out of 1712 ms in causal Flash. If the entire
saving transfers to the prefill critical path, EXP-1856's measured
3239.97 tok/s would move to about 3381 tok/s. The 3500 target requires
about 751 ms total saving from that baseline. Synthetic inputs, kernel
concurrency, clocks, and dispatch may change the actual contribution.
No model speed or memory claim follows from this probe.

To reproduce, use the exact EXP-1859 cubins checked against its SHA-256
manifest. In a directory containing `control.cubin` and `full.cubin`:

```sh
patch --silent -o sweep.py "$exp1859/probe.py" "$exp1860/sweep.patch"
python3 sweep.py
```

The next gate is an Apex A-B-B-A block with the existing overlay
executable, full context 262144, Q4_0 KV, batch and ubatch 4096, and
6290 MiB expert cache. Compare every token ID and the text, SLRU counters,
prefill, decode, request time, peak RAM and VRAM, and swap. The preflight
reported 9158 MiB free against the 9700 MiB threshold. The full-model
gate remains pending. The accepted baseline is EXP-1856.
