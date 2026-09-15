# Runtime speed hypotheses tracker — 2026-09-15

One row per candidate from `.omo/plans/runtime-speed-series.md`. Design notes are in
`experiments/designs/exp057/`. Status values: `IDEA`, `DESIGN`, `QUEUED`, `RUNNING`,
`A/B`, `ACCEPTED`, `REJECTED`, `INCONCLUSIVE`, `PARKED`, `DEAD`, `CLOSED_NO_RUN`,
`IMPLEMENTED_GATE_PASS` (candidate implemented and built, model-free gate passed; no
live model A/B yet).

Hard constraints for every row: one experiment at a time; protected trees and launcher
defaults untouched; model runs only inside an approved goal/duration/run block; one
experiment = one atomic commit; English docs.

## Candidates

| id | hypothesis | design note | phase | metric | threshold | status | verdict / evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| DEC-1 | per-node router-id readback + full scheduler synchronize is removable | D1-host-serialization | decode | paired tok/s | >=2% | IMPLEMENTED_GATE_PASS | implemented + built, model-free gate PASS (`39505119...e91510` byte-identical with `GGML_SCHED_READBACK_ASYNC` off/on, ring 9/9); live decode A/B not run, needs gate G4; `experiments/EXP-2026-09-15-065-host-serialization.md` |
| DEC-2 | IQ4_NL down-projection fusion pays at verify shape ne[1]=3 | idea | decode | microbench scan | bit-exact AND >=1.15x at Ny=3 | QUEUED (gate G1) | needs Igor go |
| DEC-3 | CPU affinity placement reduces migrations/jitter | D3-thread-affinity | decode | tok/s + jitter | >=2% or jitter >=20% with decode <=0.5% | REJECTED | median paired gain -0.201%, jitter +20% (worse); `results/archive/EXP-2026-09-15-057/`, `experiments/EXP-2026-09-15-057-cpu-affinity.md` |
| DEC-4 | no-op cudaStreamSynchronize reduction helps | idea | decode | paired tok/s | >=2% | QUEUED | overlaps DEC-1; close as duplicate if confirmed |
| DEC-5 | verify-shape kernel for ne[1]=3 | idea | decode | paired tok/s | >=2% | QUEUED (gate G1) | overlaps DEC-2 |
| DEC-6 | reduced-vocabulary MTP head lowers draft cost | idea | decode | feasibility | n/a | QUEUED (gate G2) | NOT authorized; feasibility only |
| DEC-7 | CUDA-graph L1 extension | - | decode | paired tok/s | >=2% | PARKED | EXP-052 rejected L2-only (-0.66%) |
| PRE-1 | third pinned ring slot removes part of the slot-wait union | D4-ring-depth3 | prefill | paired latency | >=3%, >=4/5, bootstrap>0 | ACCEPTED | median paired gain +11.537%, B 5/5, decode +2.247%, content byte-identical, VmSwap 0, +16 MiB pinned (48 total, within G3); `experiments/EXP-2026-09-15-062-ring-depth3.md`, `results/archive/EXP-2026-09-15-062/ab/` |
| PRE-1b | fourth pinned ring slot hides the remaining slot wait (depth-4 follow-up to PRE-1) | follow-up to D4-ring-depth3 | prefill | paired latency | >=3%, >=4/5 | REJECTED | no gain: median paired gain -4.644%, B 2/5, paired signs alternate ~+/-19% (noise-dominated); arm medians A 2977.2 ms / B 3115.5 ms = -4.644%; content byte-identical (7dfa7d1e), VmSwap 0; depth-4 pinned host 64 MiB over the approved 48 MiB budget; ring saturated at 3 slots; `experiments/EXP-2026-09-15-064-ring-depth4.md`, `results/archive/EXP-2026-09-15-064/ab/` |
| PRE-2 | GGML_OP_OFFLOAD_MIN_BATCH tuning shifts MoE CPU/GPU | D2-knob-survey | decode+prefill | tok/s + latency | >=2% / >=3% | REJECTED | no gain: best warm non-control 16 at +0.629% prefill (inside 2.4% control spread), 8 +1.936%, 4 +19.811% worse; 4096/8192 ~2x slower AND change the 395-token completion (585438c9), 4 changes greedy decode256 (eabe28ea); `results/archive/EXP-2026-09-15-058/`, `experiments/EXP-2026-09-15-058-offload-min-batch.md` |
| PRE-3 | GGML_CUDA_GRAPH_OPT reduces launch gaps | D2-knob-survey | prefill | paired latency | >=3% | REJECTED | median paired gain -5.46%, B 0/2 (both pairs slower), arm medians B +5.50%; graph-reuse window unchanged (56->72), content byte-identical (7dfa7d1e), VmSwap 0; `results/archive/EXP-2026-09-15-059/`, `experiments/EXP-2026-09-15-059-cuda-graph-opt.md` |
| PRE-4 | -ub/-b batch tuning helps prefill | D3-thread-affinity (P2) | prefill | paired latency | >=3% | REJECTED | `-b 512 -ub 256` is +57.5% slower (control median 3058.1 ms vs B median 4816.7 ms) AND changes the 395-token completion (B `09609ca7` vs control `7dfa7d1e`), so not correctness-preserving; `results/archive/EXP-2026-09-15-060/`, `experiments/EXP-2026-09-15-060-ubatch-batch.md` |
| PRE-5 | -tb batch-thread count for the gather | D3-thread-affinity (P1) | prefill | paired latency | >=3% | REJECTED | no reproducible gain: paired +0.78% and -5.32%, median -2.27%, sign inconsistent; all four content hashes `7dfa7d1e` identical; second-threadpool confound; `results/archive/EXP-2026-09-15-061/`, `experiments/EXP-2026-09-15-061-batch-threads.md` |

## Dead knobs (closed without runs; see designs/exp057/dead-levers.md)

| knob | reason | note | status |
| --- | --- | --- | --- |
| GGML_ET_UBERKERNEL / GGML_ET_PROFILE / GGML_ET_KERNELS_PATH | ET backend not built (GGML_ET=OFF) | D2-knob-survey | CLOSED_NO_RUN |
| GGML_CUDA_PDL | inert on sm_89 (needs Hopper, ptx>=90) | D2-knob-survey | CLOSED_NO_RUN |
| --poll | no-op under OpenMP build | D3-thread-affinity | CLOSED_NO_RUN |
| --spec-draft-cpu-mask | no threadpool attached to the draft context | D3-thread-affinity | CLOSED_NO_RUN |

## Decision log

- 2026-09-15: series created; 4 design notes preserved into the repo; tracker created.
- 2026-09-15: dead knobs identified; closure pending gate G5 confirmation.
- 2026-09-15: PRE-1 accepted (depth-3 pinned ring, median paired prefill gain +11.537%, B 5/5, decode +2.247%); card `experiments/EXP-2026-09-15-062-ring-depth3.md`. Default-on delivery is a separate step (Hard rule 10).
- 2026-09-15: PRE-1b rejected (depth-4 pinned ring vs the accepted depth-3 control, 5 pairs order A/B, B/A, A/B, B/A, A/B; median paired prefill gain -4.644%, B 2/5, the paired gains alternate around +/-19% so the result is noise-dominated and the point estimate is negative; arm medians depth 3 2977.2 ms / depth 4 3115.5 ms; content byte-identical `7dfa7d1e`, ring activation logged 3 vs 4 slots, VmSwap 0; depth 4 raises pinned host to 64 MiB, 16 MiB over the approved 48 MiB budget); card `experiments/EXP-2026-09-15-064-ring-depth4.md`. The ring is treated as saturated at three slots; depth 3 (PRE-1) stays, and no deeper ring is swept automatically.
- 2026-09-15: PRE-2 rejected (OFFLOAD_MIN_BATCH sweep, values 4/8/16/4096/8192 vs default 32; best warm non-control 16 at +0.629% prefill, inside the 2.4% warm-control spread; 4096/8192 ~2x slower and change the completion; 4 changes greedy decode); card `experiments/EXP-2026-09-15-058-offload-min-batch.md`. Default 32 stays.
- 2026-09-15: PRE-3 rejected (env-only `GGML_CUDA_GRAPH_OPT=1` screen, 2 pairs order A/B/B/A; median paired prefill gain -5.46%, B 0/2, both pairs slower; graph-reuse window unchanged, content byte-identical); card `experiments/EXP-2026-09-15-059-cuda-graph-opt.md`. The `GGML_CUDA_GRAPH_OPT` direction is closed; the closed two-variant capture direction (EXP-049/050/051/052) is not reopened.
- 2026-09-15: PRE-4 rejected (`-b 512 -ub 256` batch shape vs the `-b 2048 -ub 512` default, 2 pairs order A/B/B/A; control median 3058.1 ms vs B median 4816.7 ms, +57.5%, B 0/2; the B arm also changes the 395-token completion from `7dfa7d1e` to `09609ca7`, so it is not correctness-preserving); card `experiments/EXP-2026-09-15-060-ubatch-batch.md`. The `-b`/`-ub` batch-shape direction is closed for this workload and the launcher default stays untouched.
- 2026-09-15: PRE-5 rejected (`-tb 16` vs explicit `-tb 12` batch pool, 2 pairs order A/B/B/A; paired +0.78% and -5.32%, median -2.27%, sign inconsistent; arm medians B +2.20% behind A; all four content hashes `7dfa7d1e` identical; the second-threadpool confound prevents a clean attribution); card `experiments/EXP-2026-09-15-061-batch-threads.md`. The batch-thread direction is closed for this workload and the EXP-013 decode rejection stands.
- 2026-09-16: DEC-1 implemented (device-to-host async readback in the scheduler split-input fallback, `ggml/src/ggml-backend.cpp` around line 1745, gated by `GGML_SCHED_READBACK_ASYNC`, default off) and the model-free gate passed: sched output sha256 `39505119...e91510` byte-identical with the flag off and on, ring branch 9/9. No model run and no performance number; the expected 1 to 4 percent stays unmeasured and a live decode A/B needs Igor's approval under gate G4. Card `experiments/EXP-2026-09-15-065-host-serialization.md`, artifacts `results/archive/EXP-2026-09-16-065/`.

## Conclusions

- (pending per candidate)
