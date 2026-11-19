# EXP-2026-11-19-1240: P7-12 audit — the GGUF and loader ports exist and are not wired at all

## Numbers

| native file | lines | Goose touchpoints |
|---|---|---|
| `vendor/flint/src/llama-model-loader.cpp` | 1883 | **0** |
| `vendor/flint/ggml/src/gguf.cpp` | 1731 | **0** |
| `vendor/flint/src/llama-vocab.cpp` | 4947 | 19 (two doors) |

| Goose module | lines |
|---|---|
| gguf-split | 2217 |
| gguf-tensors | 829 |
| gguf-parser | 419 |
| gguf-reader | 395 |
| gguf-writer | 398 |
| goose-model-loader | 382 |
| goose-model-graph | 402 |
| gguf-metadata | 259 |
| gguf-offsets | 88 |
| **total** | **5389** |

## What the zero means

5389 lines of Goose for the GGUF and loader surface, and the native files that do that work hold **no**
reference to Goose at all. Every one of those modules is composed into the accumulated unit and therefore
counts as "in unit" to `check-unwired-modules.py`, which is why the check passes - but composed is not called.

This is worth stating plainly because the check's own wording invites the mistake: it calls a module "in unit,
so the decision is reachable". Reachable in a composed unit means the function is present and could be called;
it does not mean the shipped binary calls it. For the tokenizer the difference is visible (19 lines, two doors);
for GGUF and the loader the difference is the whole file.

## Consequence for the roadmap

P7-12 cannot be closed by more porting. The ported decisions exist; what is missing is the **call path** from
`gguf.cpp` and `llama-model-loader.cpp` into them, which is the same shape as P7-06's detokenizer problem: an
interface decision about which buffers the host hands over, not another hook.

## What would make this check honest

`check-unwired-modules.py` should distinguish "in unit" from "called by a host file", the way
`check-symbol-callers.py` does for the DSO. Until then every report of "wired" for this family needs the
touchpoint count next to it, and this card exists so the count travels with the claim.
