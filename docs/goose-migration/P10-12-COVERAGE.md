# P10-12 — complete source/entrypoint inventory and residual native logic

Status: **PARTIAL** — 3 of 3 slices accepted; 12 audit gaps remain with owners.

## Audit result

237 owned files were inspected:

| category | count | meaning |
|---|---|---|
| covered carrier `.cpp` | 15 of 19 | the decision is already Goose |
| uncovered, now ported | 3 | `file_reader.cpp`, `platform_io.cpp`, `gguf_offsets.cpp` |
| residual regions | 2 files, 7 regions | `session.cpp` (tallies, prefill plan, trace order, MTP warm-up, sampler chain) and `cli/main.cpp` (moe-predict report, driver loop) |
| substrate by decision | 3 | syscalls, GGUF walking, the llama API |
| non-production | 98 | tests, reconstructed, tools, scripts, benchmarks |
| declarative artefacts | 91 | grammar, patches, recorded data |

Entry points E1-E7 were checked: no unaccounted copy of a ported decision was
found. E2/E3/E4/E5 stay native substrate.

## Slices

| slice | content | exports |
|---|---|---|
| S1 file reader | read windows, alignment, short reads | `fr_*` |
| S2 `/proc` readers | `/proc` grammar, residency window, platform predicates | `pr_*` |
| S3 shard naming | split shard naming and resolution | `sn_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-647-p10-12-residual-io`.
- Canon: `goose/misc/{file-reader-windows,proc-readers,shard-naming}.goose`.
- Gates: 105/486 + 132/417 + 97/145 values (322 cases / 1048 values), 0 failures,
  identical at -O0.
- Mutations: 36/36 (12 per slice, halves 18/18).

## Remaining

Close the 12 open gaps (G1 tallies, G2/G3 prefill plan and trace order, G6
moe-predict report, and the rest listed in the experiment report). The list is
ranked in `REPORT.md`; the next recommended slices are G1, G6, then G2+G3.
