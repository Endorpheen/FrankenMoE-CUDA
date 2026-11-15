# EXP-2026-11-15-719 — the full matrix on the standalone that now carries Apex

Base: `2afedf7a`. Cache 4352 MiB, everything on, three warmed runs per length.

## 1. Result

| length | prefill median | decode median | decode range | reference | vs reference |
|---:|---:|---:|---|---:|---:|
| 128 | 2426.0 | 38.17 | 36.62-38.43 | 39.6 | -3.6% |
| 256 | 2421.6 | **39.50** | 39.22-39.55 | 41.0 | -3.7% |
| 512 | 2412.2 | **40.04** | 39.98-40.25 | 41.94 | -4.5% |

VRAM 7552 MiB, RAM 21.4 GiB (RSS measured on the process), content hash unchanged.

## 2. Compared with the previous accepted build

| axis | before (EXP-718) | now | change |
|---|---:|---:|---:|
| prefill @512 | 2429.9 | 2412.2 | -0.7% |
| decode @512 | 39.86 | 40.04 | **+0.5%** |
| decode @256 | not measured at 4352 | 39.50 | - |
| VRAM | 7552 | 7552 | 0% |
| RAM | 20.8 GiB | 21.4 GiB | +2.9% |

Changes of this size are inside the run-to-run spread - the 512 arm itself ranges 39.98-40.25 -
so the honest statement is **no regression and no proven gain**. What did change is structural:
the standalone binary now carries the Apex architecture, which it did not before.

## 3. What is now genuinely different

This is the first matrix measured on a standalone whose binary contains the Apex decisions
(`q35_plan`, `q35_kind`, `q35_shape`, `qg_split`, `gdn_*`, `state_slot` - 14 exports). It does not
mean the Apex path executes on Goose yet: the server that produced these numbers is still the
integrated stage-run, and the standalone is not a server.

Stating that clearly is the point. The numbers are unchanged because the serving path is
unchanged; the capability of the shipped unit is what moved.

## 4. The 128-token arm is the least stable

38.17 median with a 1.8 tok/s range, against 0.27 tok/s at 512. That was true before and is true
now, and it is why rule 20 says to judge at 512 and use 128 only to filter.

## 5. Open

- the standalone has Apex decisions but no CUDA, no tokenizer engine, no HTTP, no full-stack run;
- the integrated stage-run remains the only working end-to-end path;
- every candidate still needs this three-length matrix before acceptance.

All nine rows are in `benchmarks/experiment-history.jsonl`.
