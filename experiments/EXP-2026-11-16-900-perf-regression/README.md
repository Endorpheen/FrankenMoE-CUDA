# Open regression: 42 -> 26 tok/s. Mechanism not established.

Not closed. This file separates what was measured from what is only a hypothesis, because the first account of this
(the filled swap) was too strong for the evidence behind it.

## MEASURED

Same pinned argv, same reference hash `c2ff42cc1219`, 512 predicted tokens, three sequential requests per session.

| session | prefill ms | prefill tok/s | decode tok/s |
|---|---|---|---|
| fast (`guarded2_512`) | 14551 / 13412 / 13448 | 2251.9 / 2443.1 / 2436.6 | 41.90 / 42.23 / 42.01 |
| slow (`diag2_512`) | 41196 / 21040 / 21359 | 795.4 / 1557.4 / 1534.1 | 23.29 / 26.23 / 28.14 |

Also measured:

- **MoE cache statistics are identical** between fast and slow: same `plans=4096` line, same
  `copied_from_cache=16596 (9970.0MiB)`, same `up=` figures. The cache is doing the same work.
- `n_threads = 8` in both.
- GPU at rest: throttle reason `0x1` (idle), 46 C, 15.6 W of a 200 W limit, max SM clock 3105 MHz. No thermal or
  power throttling.
- Two builds of the same source both slow: current 26.23, and a rebuild with `build.py`/`unicode.cpp` from
  `69762a3f` at 27.44. **This weakens the recent build-change hypotheses; it does not restore the historical fast
  `.so`** with its exact objects, libraries, flags and environment.
- Swap: 32 GiB total, 84 kB free. RAM: 35 GB used, 3 GB free, 20.2 GB model.

## Signature that is informative

The slowdown is **not uniform across requests**: the first prefill is 2.8x slower, the next two plateau at 1.58x,
and decode *rises* across the three requests (23.29 -> 26.23 -> 28.14) rather than plateauing. In the fast session
all three requests were flat (41.90 / 42.23 / 42.01).

A first-request-heavy, progressively-improving pattern is what cold or evicted model pages look like: the first
pass pays for them, later passes reuse whatever stayed resident.

## HYPOTHESIS, explicitly not proven

That the model's 20.2 GB working set cannot stay resident under the current memory pressure (35 GB used, swap full,
20.2 GB file) so expert reads fall back to disk, and that this explains both the first-request penalty and the
residual 1.5x.

What would prove it and has not been done:

- a `vmstat`/`pidstat` sample **during** a slow run showing sustained swap-in or disk read on the model file -
  the sample taken after the run showed idle 87%, si 16 KiB/s, so 0, which demonstrates nothing either way;
- a control run on a quiet machine, or with the model file verified fully resident beforehand
  (`vmtouch` is not installed here);
- a comparison against the exact historical fast `.so` rather than a rebuild of similar sources.

The swap figure alone proves swap is full. It does not prove paging during inference, and it does not prove the
17.5 GB background process was equally present during the fast session.

## State

Transfer work is stopped. No new optimisations, no candidate accepted. The runtime is restored to the state
without the unicode-splitter wiring (the verified splitter module, its bridge, its 30-case differential gate and
its specification are kept as experiment artifacts, not linked into the request path). `FG_MODULES 8/8 PASS`
after the restore.

## Next diagnostic step

Sample `vmstat` and `pidstat -d` (or `/proc/<pid>/io` of the server) **during** one slow run, at the same time as
a per-request timing capture, so the paging claim is either demonstrated or dropped. One run, not a series.

## The paging hypothesis is DISPROVEN, and the swing is between identical runs

Two targeted single runs, each with instrumentation active for its whole duration. Same binary, same argv, same
reference hash, back to back.

| run | decode tok/s | prefill tok/s | paging during the run |
|---|---|---|---|
| `paging_512` (133 s) | 27.78 | 1587.4 | pswpin 65 pages = 0.3 MiB, pswpout 334 pages = 1.3 MiB |
| `gpu_512` | 38.34 | 2236.4 | not sampled, but GPU sampled instead |

So: **no paging during a slow run**. 0.3 MiB of swap-in over 133 seconds is nothing, and the hypothesis that expert
reads fall back to disk is dead. It was a reasonable guess from a full swap file; the measurement does not support
it and it is withdrawn.

And the more useful result: **the two runs differ by 38% with no code change between them.** 27.78 then 38.34, same
binary, same arguments, same hash, minutes apart. That is larger than the gap between the "fast" session (42.01)
and the slow ones (23-28).

Therefore the 42 -> 26 observation is not evidence of a code regression at all. It is the same phenomenon as the
swing between these two consecutive runs: this host's throughput varies by ~40% session to session, which matches
what was already recorded earlier in this project as session drift.

GPU during `gpu_512`: 96-97% utilisation, 2520-2850 MHz of a 3105 MHz maximum, 127-152 W of a 200 W limit, throttle
reason `0x0` except one sample at `0x4` (SW power cap). No throttling, no contention, no idling.

## What this means for the earlier conclusions

- The "regression" is not attributable to any commit, and the two-build A/B (26.23 vs 27.44) was measuring the same
  variance rather than isolating a change.
- The unchanged MoE cache statistics and unchanged hash now read consistently: the code is doing the same work, and
  the wall-clock number moves because the host does.
- What is NOT established: why this host varies by 40%. Candidates remain CPU frequency/power state, memory
  bandwidth contention from long-running processes, and GPU power-cap behaviour. None is measured yet, and the
  earlier attempt to pin it on swap was wrong.

## Consequence

A single measurement cannot decide accept/reject for this project, which is exactly what AGENTS.md rule 20 says
about paired repeats. Any future comparison here has to be interleaved and paired, not sequential, because
sequential runs on this host can differ by 40% on their own.
