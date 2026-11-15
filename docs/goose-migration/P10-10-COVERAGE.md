# P10-10 — cross-platform and alternate CPU/device support

Status: **PARTIAL** — 3 of 3 slices accepted; wiring and non-Linux branches are
open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 CPU features | feature-word decoding, x86 score, KleidiAI admission (`ggml-feats.h`, arch `cpu-feats.cpp`, `kleidiai.cpp`) | 7 `feat_*` |
| S2 platform paths | separators, current directory, executable prefix, temp dir (`common/common.cpp`) | 8 `plat_*` |
| S3 backend selection | registry order, device-name matching, fallbacks (`ggml-backend-reg.cpp`) | 10 `dev_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-648-p10-10-cross-platform`.
- Canon: `goose/misc/{cpu-features,platform-paths,backend-select}.goose`.
- Gates: 228 + 76 + 80 = 384 checks, 0 failures, byte-identical at -O0.
- Mutations: 36/36 (12 per slice, halves 6/6).
- The Linux/ARM64 decode is checked against the real `ggml-feats.h` (16 cases,
  227 fields) and the x86 score against the real
  `ggml_backend_cpu_x86_score()` (8/8).

## Remaining

Wire the three decisions into ggml-cpu, the backend registry and common paths.

## Boundaries

Apple and Windows branches are transcription only on this host (no `sysctl.h` /
`windows.h`). AVX512/AVX-VNNI/AMX accept paths are synthetic-gated because the
host reports `bits=63`, so they are not executed on real hardware here.
