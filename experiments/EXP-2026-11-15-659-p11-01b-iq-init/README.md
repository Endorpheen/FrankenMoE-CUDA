# EXP-2026-11-15-658, increment M1-b — the IQ seam removed

Base: `2fca7f4a`. Isolated tree under `/tmp/opencode/sub-661/`; the repository
was not modified. No model, server or GPU run happened.

## Results

| check | OFF | ON-all |
|---|---|---|
| build + `llama-server` link | ok, 17 794 640 B | ok, 22 050 592 B |
| accepted gates | 10/10, `goose_calls=0` | 10/10, counters match the cards |
| `goose_init` in the binaries | 0 of 11 | 1 in each of 13 |
| 55 switch exports | 0 | max 1 each |
| `op_sqrt` | absent | absent |
| init under threads | — | 512 calls → `goose_init_runs=1`, `iq_bound=1` |
| IQ views vs native tables | — | 15 360 comparisons, 0 failures |

## Seam

The unit and the adapter are now `libfg-goose-runtime.a`, a static library with
no edge to ggml. The 14 IQ views dispatch through `fg_goose_iq_tables`, a
descriptor installed at start-up (`host/goose-iq-tables.h`, ABI 1); the CPU
provider that owns the file-static tables sits in `ggml-quants.c` and is
referenced weakly. The target graph has 0 cycles and needs no `--start-group`.

The S1.2 and S1.3 bridges lost their private init guards, so the descriptor is
installed before the only `goose_init()` in the process.

## Files

- `REPORT.md` — full English report (contract, commands, gates, nm, graph,
  hashes, deviations, remaining work).
- `gen_host.py` — emits the adapter (65 bodies incl. the 14 IQ dispatchers).
- `tree/host/goose_host.c`, `tree/host/goose-host.h`,
  `tree/host/goose-iq-tables.h` — the adapter and the contract.
- `run-off/`, `run-on/`, `audit/` — gate outputs, nm tables, target graph,
  archive audit, SHA-256 lists.

## Boundaries

Build-only. No performance or correctness claim about decoding is made; the
A/B protocol in `docs/goose-migration/APEX-TEST-PLAN.md` needs Igor's approval.
