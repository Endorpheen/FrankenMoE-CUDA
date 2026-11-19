# The remaining native primitives, each with why it is one

P10-12's acceptance is that every implementation path and public entry point maps either to a completed Goose task or to an
audited primitive, and that nothing is exempt by silence. This is that mapping; it is the document the requirement asks
for, not a plan.

## Audited primitives that stay native

| primitive | where | why it stays | how it is audited |
|---|---|---|---|
| the GGUF container walk | `ggml/src/gguf.cpp` | the port's `gr_parse` reads the same bytes and is compared against it: every key index, key type, integer and float value (bit for bit), string value, array length and 744 231 array elements are checked at load, and a disagreement refuses the load | `FG_GGUF_*` counters, EXP-1397, -1400, -1401 |
| the compute substrate | `ggml/` | tensors, kernels, quantisation; the project treats substrate as not-debt by rule | not applicable |
| C++ standard library and libc | - | the runtime is C++; rule 27's no-libstdc++ goal is a destination, not the current state | - |

## Paths that are ported and served

| path | served since | evidence |
|---|---|---|
| GGUF metadata reads in the loader | verified, not served - the loader still calls the native API after the check | `FG_GGUF_LOOKUP` |
| tokeniser encode (`llama_tokenize`) | EXP-1394, default since EXP-1396 | 248 320 token texts equal, text-prompt acceptance hash identical |
| detokeniser (piece cache, buffer, EOG, sequence) | EXP-1351 … EXP-1358 | 24 933 calls equal, then served |
| sampling chain doors | accepted stages | per-door cards |
| expert-cache replacement policy | **compared, not served** | 61 441 decisions equal, EXP-1419 |

## Paths that are ported and reachable but not wired

| door | composed | call site |
|---|---|---|
| fit-params line (`fp_line`) | EXP-1406 | none in this tree - the tool is not built here |
| bench report (`lb_*`) | EXP-1408 | none |
| CUDA arch policy, flags, install layout | EXP-1425, -1426 | build system, not runtime |
| HF cache naming (`hf_*`) | EXP-1430 | `common/hf-cache.cpp` exists, wiring not done |

## Artefacts that are not on the runtime path

| artefact | state |
|---|---|
| the reconstruction harness | not linked into any shipped binary (`nm` and `ldd` both empty for it) |
| `experiments/*` stands | separate builds, never linked into the product |
| checkpoints under `builds/checkpoint-*` | preserved copies, not linked |

## What is explicitly NOT exempt

No file under `vendor/flint/src` is excluded from this mapping by category. Each one is either substrate, a served path, a
compared path, or an unwired door - and the last two are listed above with their status.

## The measurement behind the claim

```
python3 tools/check-accepted-components.py   -> 21 entries, all present
bash tools/check-unresolved-symbols.sh       -> ok
python3 tools/audit-door-evidence.py         -> 49 doors, 0 with no record
```
