# P9-03 — dense weights and expert streaming/LRU sources

Status: **PARTIAL** — 3 of 3 slices accepted; 25 call sites are still native.

## Slices

| slice | content | exports |
|---|---|---|
| S1 expert LRU and residency | touch/commit contract, promotion, eviction, prefetch filter, speculative reads, cache budget | 14 `xl_*` |
| S2 dense placement | range complement, MemAvailable filter, row tables, dense mode verdict | 6 `dw_*` |
| S3 row-stream plan | slab plan, generation guard on eviction, read order, materialise bound | 7 `sp_*` |

The experiment ships one composite unit because three generated headers cannot
share one translation unit. The composite is a build artefact, not a canonical
module: `goose/model/` carries the three source modules and the composer
rebuilds the unit.

## Evidence

- Experiment: `experiments/EXP-2026-11-15-621-p9-03-stream-policy`.
- Canon: `goose/model/{expert-lru,dense-place,stream-plan}.goose`.
- Gate: 74 cases / 198 checks / 0 failures against the native transcription;
  the same gate passes at -O0.
- Mutations: 12/12 in two halves.

## Remaining

Replace the 25 call sites in the expert cache, dense placement and row
streaming. Contracts to keep: `xl_touch` takes an already-committed access,
`xl_set_budget` returns the budget, and `quiesce_spec()` stays with the caller on
the compression branch.

## Boundaries

File/disk (`FileReader`, `O_DIRECT`, `pread`), the kernel (`vm_*`, `madvise`,
`mincore`, `/proc`, pinned pages), threads, the graph/ABI and telemetry stay
native. Honest conflicts: u8->i64 range widening (+~256 KiB at 40x128),
`std::list` replaced by a flat array in `sp_evict`, a 32-bit generation mask, and
a transcription oracle.
