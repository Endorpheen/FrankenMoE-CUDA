# Resolved: the paired Apex measurement for EXP239–242

Status updated 2026-11-11. The blocker described below is fixed and the pair now exists. The earlier notes are
kept because the failure modes are the reusable part.

## The pair

`exp238` (before the batch-port block) against `exp242` (after it), order old → new → new → old, only
`libllama.so.0.7.0` swapped between runs. Tokens and all 128 saved probabilities identical in every run
(`sha e68df05b`).

| run | prefill tok/s | decode tok/s | tokens | probs |
|---|---:|---:|---|---|
| `o1` (exp238) | 2494.40 | 24.313 | exact | exact |
| `n1` (exp242) | 2492.11 | 25.483 | exact | exact |
| `n2` (exp242) | 2502.81 | 23.845 | exact | exact |
| `o2` (exp238) | 2492.63 | 24.780 | exact | exact |
| **old mean** | **2493.52** | **24.547** | | |
| **new mean** | **2497.46** | **24.664** | | |
| **delta** | **+0.16%** | **+0.48%** | | |

No regression. Both deltas are positive and inside the 2% threshold.

## The caveat that matters more than the verdict

Run-to-run spread within a four-run series:

| group | spread |
|---|---:|
| old prefill | 0.07% |
| new prefill | 0.43% |
| old decode | 1.92% |
| **new decode** | **6.87%** |

**Decode cannot resolve a 2% regression in this environment.** The two `exp242` decode runs differ by 6.87%,
which is three times the threshold the rule asks me to enforce. A single pair showing `n2`'s 23.845 next to
`o2`'s 24.780 would read as −3.8% and would be pure noise.

So the honest statement is: **prefill is measured and clean (+0.16% with 0.43% spread); decode is
indeterminate.** Four runs can bound a decode regression at roughly ±4%, not ±2%. Enforcing the 2% rule on
decode needs either more repetitions or a quieter host, and until then no decode verdict should be quoted
from a single pair.

## What the earlier failures were

1. **Disk.** Host was at 100% (`888G/938G`). Copying both stage `run` directories took it to `131M` free and
   `cp` failed part-way through a directory — which silently produced a **truncated 174M `run` directory**
   instead of an error I noticed. Every subsequent run failed inside the runner with
   `No such file: .../libggml.so`. Removing `runtime/exp240/run{_old,2,3,4}` (5 × 637M) and my `/tmp` scratch
   returned 6.0G.
2. **`setsid` without `--wait`** returned immediately, so the next run started while the previous server still
   held port 8081.
3. **The runner hardcoded `runtime/exp242/results` and `.../measured`,** so `FG_SUFFIX` changed only the label
   and every run overwrote one `summary.json`. A fifth run then returned figures identical to two decimals with
   the previous one — which is what exposed it. Fixed by deriving both paths from `FG_STAGE` plus the label.

## Rule for future pairs

- one `FG_STAGE` per measurement campaign, distinct `FG_SUFFIX` per run, and the runner must derive **both**
  results and measured roots from them;
- switch DSOs by symlink, never by copying a `run` directory — the copy is 637M and a partial one is invisible
  until the runner fails several minutes later;
- check `du -sh` against the known 648M before trusting a stage directory;
- verify the port is free before each run instead of trusting `setsid`.
