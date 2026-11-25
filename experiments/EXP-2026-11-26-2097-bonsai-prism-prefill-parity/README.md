# EXP-2026-11-26-2097 — Contemporary Bonsai prefill comparison with PrismML

**Diagnostic comparison with no runtime changes.** Compare packaged
EXP-2096 to the same SM89 PrismML binary and inference libraries as
EXP-2091. Run sequentially on RTX 4070 in the order PrismML, Goose,
Goose, PrismML, excluding profiler overhead. A reverse pair was
added because the initial prefill difference was only 389 ms.

All four valid runs use the same GGUF SHA-256, EXP-1977 raw request,
32,768 input IDs, 512 returned IDs, context 36864, batch/ubatch
4096/1024, Q4_0 K/V, greedy, seed 123, EOS masked, MTP off, and
the explicit CPU thread and mask settings. All four pass the
cold-prefill gate and match the archived PrismML 512 IDs and text.
The effective runtime arguments, environment, GPU readings at
launch and build hashes are recorded in the receipt. GPU clocks
were unlocked, and sampling was limited to prelaunch readings.
Process VRAM uses post-request NVML readings. VmHWM includes startup.

## Results

| Arm | Prefill, tok/s | Decode, tok/s | Prefill, s | Decode, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| PrismML P | 1100.43 | 48.28 | 29.777 | 10.584 | 40.371 | 8288 | 7654296 |
| Goose G | 1086.23 | 37.59 | 30.167 | 13.595 | 43.788 | 8932 | 7457364 |
| Goose G2 | 1102.15 | 38.05 | 29.731 | 13.431 | 43.186 | 8932 | 7458492 |
| PrismML P2 | 1089.82 | 47.88 | 30.067 | 10.672 | 40.748 | 8288 | 7653948 |
| Mean PrismML | 1095.12 | 48.08 | 29.922 | 10.628 | 40.559 | 8288 | 7654122 |
| Mean Goose | 1094.19 | 37.82 | 29.949 | 13.513 | 43.487 | 8932 | 7457928 |

Both runtimes' reported decode rates equal 511 post-first-token
steps divided by the recorded decode interval. The prefill
difference, Goose minus PrismML, is +389 ms in the first pair and
−336 ms in the reverse pair. The mean is +27 ms (−0.086%
throughput). This is practical prefill parity for this block, with
no demonstrated sustained lead or formal equivalence bound.
Goose takes 2.885 s longer in decode (21.35% lower throughput)
and 2.928 s longer per request. Post-request process VRAM is 644
MiB higher. Process swap is zero for all arms.

With this contemporary PrismML mean, the +70% targets on this
profile are approximately 1861.71 prefill and 81.74 decode tok/s.
This block covers the fixed workload and does not resolve the
experimental line's broader quality gate. No model or code change
was made in this comparison.

## Execution evidence and next direction

[EXP-2096](../EXP-2026-11-26-2096-bonsai-ffn-down-fused-q8/data/attribution.json)
provides the identical Goose build's full kernel inventory.
[EXP-2091](../EXP-2026-11-26-2091-bonsai-prism-full-comparison/data/attribution.json)
provides the matching PrismML build's full inventory. There was no
new CUDA or CPU profiling in the scored block. Focus the next
decode investigation on the still-active direct Q4 MMA path and
use CPU profiling to attribute the smaller host-side gap separately.
On prefill, keep the full decode and quality gates while targeting
PQ2 and the long Attention path to move beyond parity.

Re-use the EXP-1977 PrismML command and the staged EXP-2096 profile
with a fresh process for each arm. Use the same raw request file,
with `tools/validate-cold-32k.py before` and `after`. The unmodified
local log, response and process snapshots are under
`build/bonsai-prism-parity-2097`.

[Full measured arms, computed denominators and artifact hashes](data/results.json).
