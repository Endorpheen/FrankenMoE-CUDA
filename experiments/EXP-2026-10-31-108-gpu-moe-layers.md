# EXP-2026-10-31-108: additional GPU MoE layers from freed VRAM

Status: REJECTED for the default profile. 40-CPU profile kept.

Control EXP101 `--cpu-moe`; B `--n-cpu-moe 38` (2 GPU expert layers); C
`--n-cpu-moe 35` (5 GPU expert layers); lazy KV on. One screen per profile then
five balanced A/C pairs.

A/B/C sampled VRAM 3188 / 4184 / 5678 MiB. A/C medians: prefill +7.08% (5/5),
cold decode +4.82% (5/5), repeat decode +4.91% (5/5), VRAM cost +2490 MiB.
Numeric: first selected-ID divergence at position 90, max top-5 probability
delta 0.61; not an equivalent replacement. Decode gain is below the 5% gate and
the memory price is high, so the minimal-memory 40-CPU profile stays default.
Artifacts: results/opencode-next/EXP108/.
