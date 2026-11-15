# P10-02 — maintained command-line tools and utilities

Status: **PARTIAL** — 3 of 3 slices accepted; the host wiring is open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 llama-bench reporting | output format and modes, `cmd_params_defaults`, `parse_int_range`, `avg`/`stdev`, field types, JSON/CSV/SQL escaping, markdown width/units, `tensor_split` | `lb_*` |
| S2 llama-tokenize | prompt source priority (stdin > file > `-p`), escape application, `add_bos`, `--ids` and piece output, `--show-count` | `tz_*` |
| S3 fit-params line | the printable `-c/-ngl/-ts/-ot` line, zero-split trim, `uint32` conversion | `fp_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-638-p10-02-cli-tools`.
- Canon: `goose/cli/{llama-bench-report,tokenize-cli,fit-params-line}.goose`.
- Gates: 182/503 + 40/40 + 28/28 values, 0 failures, identical at -O0.
- Mutations: 36/36 (12 per slice, halves 6/6).
- The native oracle is the real `llama-bench.cpp` translation unit (main
  dropped, four host facts stubbed); tokenize and fit-params use a transcription
  with exact printf formats because their mains require a model.

## Remaining

Wire the three decisions into their hosts: the llama-bench report, the tokenize
prompt/output path and the fit-params line.

## Boundaries

`u64` fields cross the ABI as `i64` (< 2^63), `f32` splits widen to `f64`,
`fp_u32` is defined on `[0, 2^32)`, the Windows UTF-8 branch is modelled by a
flag, and `--help`/`--version`/exit stay with the host. `gguf-split`, the CLI
config surface and batched-bench were checked and not duplicated.
