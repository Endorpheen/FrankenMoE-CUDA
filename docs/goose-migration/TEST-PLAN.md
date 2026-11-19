# Test coverage plan: the 24 modules of the shipped unit, progress and remainder

The unit is 24 modules. "Done" means a test drives the shipped call path and checks the answer; "through" means the module is
exercised by another module's test; "to complete" is what is left.

## Progress: 23 of 24 measured

| # | module | test | state |
|---|---|---|---|
| 1 | `sampling/sampler-chain-full` | `EXP-2026-11-19-1447` sampler differential, 9 000 cases | done |
| 2 | `speculative/spec-doors` | spec experiments, 880 rejection combinations | done |
| 3 | `speculative/spec-decisions` | body, exercised through the doors | through |
| 4 | `misc/spec-decode-decisions` | `EXP-2026-11-19-1408`, through the doors | through |
| 5 | `misc/unicode-tables` | `EXP-2026-11-12-289`, `EXP-2026-11-17-891` | done |
| 6 | `misc/unicode-text` | through `pretokenizer-qwen35` and `bpe-detokenize` | through |
| 7 | `misc/pretokenizer-qwen35` | `EXP-2026-11-19-1442`, full vocabulary | done |
| 8 | `model/bpe-tokenize` | `EXP-2026-11-19-1442`, 23 prompts, served equals native | done |
| 9 | `model/bpe-detokenize` | `EXP-2026-11-19-1441/1443/1448/1449` | done |
| 10 | `model/tokenizer-decisions` | body, exercised through the doors | through |
| 11 | `model/tokenizer-doors` | `EXP-2026-11-19-1449` boundaries | done |
| 12 | `misc/gguf-headers` | through `gguf-reader` | through |
| 13 | `model/gguf-tensors` | writer cases relinked against the shipped library | done |
| 14 | `model/gguf-reader` | 744 231 elements, every key and type | done |
| 15 | `cli/fit-params-line` | `EXP-2026-11-19-1407`, 20 cases | done |
| 16 | `cli/llama-bench-report` | `EXP-2026-11-19-1461`, 24 cases | done |
| 17 | `cli/config-validate` | 4 000 cases, 0 uncovered | done |
| 18 | `model/slru-policy-goose` | `EXP-2026-11-19-1422/1423`, 61 441 decisions | done |
| 19 | `model/arch-registry` | `EXP-2026-11-19-1456`, round trip over every row | done |
| 20 | `cuda/cuda-arch-policy` | `EXP-2026-11-19-1456/1457` | done |
| 21 | `cuda/cuda-target-flags` | `EXP-2026-11-19-1457`, 6 branch cases | done |
| 22 | `cuda/cuda-install-layout` | `EXP-2026-11-19-1459`, 20 cases as a table | done |
| 23 | `server/hf-cache-naming` | `EXP-2026-11-19-1460`, 29 cases as a table | done |
| 24 | `metrics/metrics-traces` | `EXP-2026-11-15-618`, staged harness | **to complete** |

## Remainder: 1 module

4. `metrics/metrics-traces` - 21 exports. Needs the CSV field rules and trace weight kinds compared with the native
   writer over generated rows.

## Cross-cutting gates, already in place

| gate | what it holds | state |
|---|---|---|
| `tools/awt/run.sh` | agent session, tools, streaming, liveness after every step | done |
| `tools/awt/check-vram.py` | peak and growth against a measured baseline | done |
| AWT `hostile` / `refuse` / `repeat` | bad input, refusal and recovery, determinism | done |
| `tools/check-accepted-components.py` | every accepted component still in the built library | done |
| `tools/audit-door-evidence.py --modules` | no module without evidence | done |

## Order: four modules, then integration

1. `metrics-traces`

After this one, no module of the unit is without a test that drives its doors, and the work returns to the runtime
integration: the paired A/B on a quiet card, the checkpoint, and the remaining requirement items.
