# EXP-2026-09-08-045 — Two-thread CPU gather (model-free prototype)

Status: `INCONCLUSIVE` — the series was terminated by Igor's decision after four full pairs; the original gate is not confirmed, integration and further measurements are not performed.
Kind: `kind=prototype-offline` (no model; no performance claim about the server is made)

## Hypothesis

The scheduler thread and one permanent helper thread can simultaneously copy
non-overlapping parts of one range into a pinned slot, reducing transfer preparation
time.

Expected benefit — shorter prefill and shorter wait for the first response. Basis EXP-043:
gather took 1463.641 ms, and 496.453 ms of GPU idle time coincided with gather. These
496.5 ms are an observed segment, not a guaranteed win. No decode speedup is promised.

The practical reference point for a future model-based candidate is ≥3% prefill (roadmap
decision rule).

## The single variable

- A: one thread, plain `memcpy` into the slot (the path of accepted EXP-038/041).
- B: two threads total — the calling thread and one permanent helper thread.

Kept unchanged: two pinned slots of 16 MiB each, the original ranges, padding,
the number and volume of H2D transfers, CUDA submission order, the existing single stream,
`cudaEventDisableTiming`, the order "slot wait → both CPU copies complete → H2D → event".

Not added at the same time: new memcpy instructions (non temporal, SIMD), CPU affinity,
an additional stream, a larger ring, merging ranges, checkpoint, limited-gap, MTP.

## Workload (representative, no inference)

The source of accesses is the saved EXP-032 trace:
`results/archive/EXP-2026-09-06-034/raw/exp032-h2d-ts/h2d.log`
(LOCAL_ONLY on disk; sha256 `3267cbeaeec4f25fd328e88db8b882f73b2b307ad0913d452b6aa56577b55945`).

Addresses are reconstructed unambiguously without inference: the CUDA tensor name from the
log (`CUDA0#<name>#<idx>`) → the GGUF tensor name → `data_offset` from the shard metadata
(`work/llama.cpp-exp041/gguf-py`, standard GGUF v3 numbering, offsets absolute within the
file). Model shards (read-only, `mmap PROT_READ MAP_SHARED`):

- `models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00002-of-00003.gguf`
- `models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00003-of-00003.gguf`

Generator: `results/archive/EXP-2026-09-08-045/gen_exp045_ranges.py` →
`results/archive/EXP-2026-09-08-045/exp045_ranges.txt` (format `shard abs_offset bytes`).

Mapping check: 17886/17886 records matched, 0 unmatched, 0 accesses beyond tensor
boundaries. Volume: 17886 ranges, 21.641 GiB total, all 17886 unique by start address;
min 524800 B, p50 922112 B, max 11981312 B. Source footprint 21.6 GiB ≫ L3 64 MiB —
the condition "not a single buffer in the CPU cache" is genuinely satisfied, no synthetic
workload is needed.

Transfer limitation: the EXP-032 trace was recorded on the pageable path (EXP-031); volumes
match EXP-043 byte-for-byte by the number of calls (17886); the order of ranges is taken
from this trace.

## Prototype

Source directory: `results/archive/EXP-2026-09-08-045/` (separate from the runtime).
Build directory: `build/exp045-gather-2t/` (separate; the working
`build/exp041-default-runtime` is not touched). `work/llama.cpp-integration`, EXP-041,
launcher — unchanged.

`exp045_gather2t.cpp` reproduces the loop of
`ggml_backend_cuda_set_tensor_async_pinned_ring`:
`chunk = min(remaining, 16 MiB)`, 2 slots, `cudaEventSynchronize` before reuse,
`cudaMemcpyAsync` + `cudaEventRecord` on the calling thread, one stream.

Mode B: the helper is created once outside the measured part and lives for the entire run.
The chunk assignment is published by the calling thread (payload field → release-store of
the counter), the split boundary is rounded down to 64 B (cache line): the caller copies
the first part, the helper the second (non-overlapping intervals in both source and
destination). The calling thread always waits for the completion publications of both
parts before `cudaMemcpyAsync` (accept-load of the completion counter). Order: wait for
slot release → both CPU copies → H2D → event. Assignment ownership — a single producer
(the caller); completion — release/acquire; shutdown — acquire of the quit flag + `join`
before `munmap` of the source and before releasing the slots (source/slot lifetime).
CUDA calls only on the calling thread.

H2D destination in the prototype: a bounded-size device arena (default 256 MiB; the actual
runs use `--arena 64`, because on the 12 GiB card Igor's server occupies 9.2 GiB) with
cursor wrap, so as not to require 21.6 GiB of VRAM on a 12 GiB card and not to disturb
Igor's server. This does not change bytes/calls/order/sizes of transfers; H2D cost does
not depend on the address in VRAM. The byte-for-byte gather verification is performed on
the pinned slot (this is exactly the object of the experiment) plus a device round-trip on
a separate non-wrap selftest region.

## Correctness (before measurements)

`--selftest` (always activates the two-threaded path; the mere fact that it runs does not
count as a gate):

1. byte-for-byte destination match with A after synchronize (hash of each chunk A vs B);
2. immutability of the source and sentinel areas (source checksum before/after, 0xCC past
   the end of the chunk);
3. odd sizes (1, 63, 65, 127, 524801), unaligned addresses (+1, +3), 16 MiB boundaries
   (16 MiB−1, 16 MiB, 16 MiB+1), splitting of large ranges (17 MiB, 33 MiB — several
   chunks);
4. repeated reuse of both slots (3 repetitions of the case list);
5. helper termination without touching freed memory (join before `munmap`; the helper
   stops immediately after a completed assignment).

Additionally `--verify` on the real range list: an FNV-1a hash of the slot contents after
each gather copy, the full list A against the full list B (17886 hashes, byte-for-byte
verification).

## Measurements

Manually, strictly sequential, no parallel runs and no background builds.
First one preliminary A/B pair. If there is no useful signal or B is worse — stop, without
an automatic sweep of the thread count and thresholds. On a positive signal —
5 consecutive pairs alternating A/B and B/A.

Warmup: one full pass over the range list (excluded from the averages) — warms up
page-cache page touching; duration estimated at ~3–4 s per run (21.641 GiB gather + H2D).
Swap is not provoked; the model is not evicted from the page cache (the same model pages
are read).

Primary metric: total time of the gather + H2D sequence with a final drain
(`cudaStreamSynchronize` after the last event). Additionally:
gather wall time (including handing over the assignment and waiting for the helper), slot
wait time, submit, actual bytes/calls/chunks, CPU time (caller/helper/process),
RSS/VmHWM/VmSwap, results by size groups (<600 KiB, 600 KiB–1.2 MiB, 1.2–2.5 MiB,
≥2.5 MiB).

The CPU time of two threads is not summed as wall time. Synchronization is not excluded
from the cost of B. Copy instrumentation is identical for A and B. Before the measurements
the senior helper fixed the A control: no helper is created. The original helper with no
assignments was actively spinning in pause/sched_yield and consuming CPU, which production
A does not have. B keeps one permanent helper created outside the timed passes.

## Criterion for promoting to a server candidate

- median reduction of the total model-free sequence time ≥5%;
- wins in at least 4 of 5 pairs;
- all byte-checks pass;
- VmSwap = 0.

This is only grounds to propose integration, not proof of a server speedup.
If only the isolated memcpy is faster while the full gather+H2D does not speed up — the
gate is not passed. Report the helper's additional memory (8 MiB stack by default, thread
data) and the risk of contention with CPU-MoE, the scheduler, and other threads.

A model run, integration into the accepted runtime, and a performance claim are not
authorized by this spec. Before the server — the goal, duration, number of runs, and an
explicit "yes" from Igor.

## Stage 1 artifacts (code + workload)

- `results/archive/EXP-2026-09-08-045/exp045_gather2t.cpp` — sha256 `2a07e3e267556126…`
- `results/archive/EXP-2026-09-08-045/gen_exp045_ranges.py` — sha256 `864b7d74e9d338f7…`
- `results/archive/EXP-2026-09-08-045/exp045_ranges.txt` — sha256 `8b58328ed61b0bfe…`
  (17886 lines `shard abs_offset bytes`)
- `results/archive/EXP-2026-09-08-045/build.sh` — builds into
  `build/exp045-gather-2t/exp045_gather2t`
  (nvcc 12.6, `-O2 -std=c++17`) — binary sha256 `16e8559e4b9585bc…`
- `results/archive/EXP-2026-09-08-045/run_ab.sh` — one manual run of one arm (64 MiB
  arena, warmup 1, passes 1); arms are run strictly in turn.

## Correctness — done (2026-09-08, no model)

`--selftest` — 8/8 PASS: `st2_slot_tail_untouched_A/B`, `st3_mode_A_all_checks`
(per-chunk memcmp of source against slot), `st3_mode_B_all_checks` (the same on the
two-threaded path), `st4_slot_bytes_B_equals_A` (hashes of all chunks A == B),
`st5_source_unchanged`, `st6_device_roundtrip_equals_source`,
`st7_helper_stopped_before_munmap`. Cases: 1, 63, 64, 65, 127, 524800, 524801,
16 MiB−1, 16 MiB, 16 MiB+1, 16 MiB+65, 17 MiB, 33 MiB (several chunks), unaligned offsets
0/1/3, three repetitions (reuse of both slots).

Byte-for-byte verification on the real range list (17886 chunks, 23 237 033 984 B):
`--hash` A and `--hash` B produced identical hash lists — sha256 of both files
`5bbf98f3f18049105f5fe7e758a6bda033c67d2fc1accf3bf5b7b528e90dcde8`. VmSwap=0, RSS 23.5 GiB
(model weight page touching), no swap.

## Measurements — done (2026-09-08)

The window was agreed: Igor stopped his personal server (port 8081), the GPU had no
foreign load. The measurements were performed by the senior helper manually, strictly
sequentially, via `run_ab.sh` (warmup 1 + 1 measured run per arm). One preliminary pair
and four formal pairs with alternating order were completed. The fifth pair was not run —
the series was stopped by Igor's decision after four full pairs; repeated measurements
only by his new decision.

## Results — final summary (replaces all intermediate notes)

All measured runs are identical in workload: 17 886 calls/chunks, 23 237 033 984 bytes,
VmSwap=0, RSS 23 537.8–23 542.3 MiB (mostly the touched mmap pages of the model).
Copy instrumentation is identical for A and B.

Preliminary pair (authorization for the formal series; not included in the statistics):
A 1930.754 → B 1767.252 ms, −8.47% wall. Artifacts: `review-exploratory-{A,B}.json`.

Formal pairs (total gather+H2D time with final drain, ms; `review-formal-*.json`):

| Pair | A, ms | B, ms | Δ wall |
| --- | --- | --- | --- |
| 1 | 1938.043 | 1873.906 | −3.31% |
| 2 | 1934.857 | 1762.961 | −8.88% |
| 3 | 1891.796 | 1989.384 | +5.16% (B regression) |
| 4 | 1914.073 | 1781.153 | −6.94% |

Result of four full pairs: medians A 1924.465 → B 1827.530 ms (−5.04%); median pairwise
reduction 5.13%; B wins — 3 of 4; the pair 3 regression (+5.16%) was not excluded.
Additionally: gather median 1507.416 → 1355.970 ms (−10.07%); slotwait median
366.683 → 402.284 ms (in B the slots more often wait for H2D completion). The decisive
metric is total time.

CPU accounting caveats: B uses one extra CPU thread; helper_cpu_ms
(1264–1501 ms) accounts for the helper's work inside assignments, not the whole spin;
proc_cpu_ms is cumulative and includes warmup — not the CPU time of the measured pass.
Synchronization was not excluded from the cost of B.

The gate for promoting to a server candidate (median ≥5% AND ≥4/5 wins AND byte-checks
AND VmSwap=0) is not confirmed: the 5.04% median is formally reached, but there are only
3/4 wins with the series stopped, and there is one regressing pair. Byte-checks and
VmSwap=0 — no remarks.

## Stage 2 artifacts (A-control fix, rebuild, measurements)

- `exp045_gather2t.cpp` — fixed version: in A no helper is created (originally the helper
  with no assignments was actively spinning in pause/sched_yield, which production A does
  not have), in B the helper is a single one and is created outside the measured part.
  The file in the archive was replaced with this version,
  sha256 `f4573d331d1d4c07af15c6dbde0b2cb8f3b5517a04f71ef7f283fd53d00b1fa6`;
  the stage 1 hashes (`2a07e3e2…`, binary `16e8559e…`) referred to the pre-fix version.
- `build/exp045-gather-2t/exp045_gather2t` — rebuilt after the fix (nvcc 12.6,
  `-O2 -std=c++17`), sha256 `5dbe377b8838cbb5c0f98069f592bb0bbc5a50b9fcdf8caa16af9f07724dd241`;
  `build/` is in gitignore, the binary is not committed and is reproduced by `build.sh`.
- `review-selftest.txt` — repeated selftest after the rebuild: 8/8 PASS (byte parity and
  device round-trip included). The full byte-for-byte verification on 17 886 real ranges
  (sha256 of the hash lists `5bbf98f3…`, section "Correctness — done") was done before
  the fix; the fix did not change the copying algorithm (only the idle helper was removed
  in A), parity after the rebuild was rechecked by the selftest.
- `review-exploratory-{A,B}.json`, `review-formal-{1,2,3,4}-{A,B}.json` — all measured
  runs of the series.

## Conclusion

INCONCLUSIVE for promotion to integration. There is a positive signal (medians −5.04%,
pairwise median −5.13%, 3/4 wins), but the original gate is not met: the series was
stopped by Igor's decision after four full pairs, and one pair gave a +5.16% regression.
Model correctness and the candidate's server speedup were not verified; the model-free
prototype numbers do not relate to server tok/s.

A separate server candidate is not proposed; the direction is closed without automatic
redesign and without porting into the accepted baseline: the risk of the extra thread
contending with CPU-MoE and the scheduler in the real server has not been measured. The
fifth pair, repeated measurements, and any baseline changes — only by Igor's explicit
decision. The next focus per his decision of 2026-09-08 — decode speedup; no new prefill
experiments are scheduled.
