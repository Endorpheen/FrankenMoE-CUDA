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
