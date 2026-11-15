# Architecture dependency map (AGENTS.md rule 17)

Base: `caa7edac`. Audit only; no build.

Apex is one architecture among many. This map exists so that a working Apex is
never obtained by destroying the rest, and so that every removal can be checked
against what the unported architectures still need.

## 1. The scale

| item | count |
|---|---:|
| architectures the substrate implements (`upstream/llama.cpp/src/models/*.cpp`) | 151 |
| architectures with a Goose builder today | 13 |

The migration is therefore 13 of 151 architectures, and Apex is one of the 13. Any
statement that "the migration is nearly done" refers to Apex only.

## 2. What is shared, and must stay architecture-neutral

These carry no per-architecture decision. They are the pieces an unported
architecture needs, so none of them may acquire an Apex shortcut:

| shared layer | modules | owners |
|---|---|---|
| GGUF container | substrate reader | staysnative |
| GGUF metadata / tensors / offsets | `gguf-metadata`, `gguf-tensors`, `gguf-offsets`, `gguf-split`, `gguf-writer` | Goose |
| quantisation | `quant-types`, `quant-allow`, `quant-rows`, `quant-rows2`, `quant-dequant`, `quant-dequant2`, `quant-weighted` | Goose |
| file I/O decisions | `file-io`, `shards` | Goose |
| model family base | `goose-model-graph` (the `base.model_graph` alias) | Goose |
| architecture registry | `arch-registry` | Goose |
| graph construction | `cpu/goose-graph-builders` (16 primitives, 28 modes) | Goose |
| expert residency | `expert-lru`, `dense-place`, `stream-plan` | Goose |
| router hooks / overlap / row select | `hook-rules`, `overlap-rules`, `row-select` | Goose |
| backend seam | public `ggml-backend` API | substrate + Goose decisions |
| server / API | `server/*`, `chat/*` | Goose |

## 3. What is per-architecture, and must not leak

| architecture | builder | exports | imports base |
|---|---|---:|---|
| qwen35moe (Apex 35B-A3B) | `qwen35moe.goose` | 3 | yes |
| qwen3next | `qwen3next.goose` | 9 | yes |
| deepseek4 | `deepseek4.goose` | 25 | yes |
| deepseek32 | `deepseek32.goose` | 10 | yes |
| gemma3 | `gemma3.goose` | 19 | yes |
| gemma3n | `gemma3n.goose` | 24 | yes |
| gemma4 | `gemma4.goose` | 12 | yes |
| glm4 | `glm4.goose` | 13 | no |
| glm-dsa | `glm-dsa.goose` | 24 | yes |
| glm5-next | `glm5-next.goose` | 30 | yes |
| kimi-k3 | `kimi-k3.goose` | 24 | yes |
| kimi-linear | `kimi-linear.goose` | 26 | yes |
| mistral3 | `mistral3.goose` | 9 | yes |

One builder per architecture, one unit per architecture, and the builders are never
composed together: they are alternatives, and two of them in one unit is a
conflict, not a merge.

## 4. The Apex-specific pieces, and why they are not in the common unit

- `qwen35moe.goose` - only Apex's per-layer kinds and tensor extents.
- `tools/build-arch-unit.sh <arch>` - parameterised by architecture since this
  audit; before it, the script hardcoded Apex, which is exactly the kind of
  shortcut rule 17 forbids. The module list still comes from the common build.

Nothing else in the repository knows about Apex. The graph builders, the metadata
modules, the allocator arithmetic and the bindings are architecture-neutral, and
the Apex unit is produced by the same script any other architecture uses.

## 5. Multi-architecture composition check

Verified with the local gate (compose only, no C compiler):

| architecture | composes with the common set |
|---|---|
| qwen35moe | yes (120 modules, 3991 specializations) |
| deepseek4 | yes |
| gemma3 | yes |
| mistral3 | yes |
| glm4 | **no** - `extern fail` binds a different symbol than the canonical one |

The `glm4` case is the same class of collision `qwen35moe` had before the alias:
its `fail` binds `fg<N>_fail` while the common set binds another. It needs the same
treatment - a scoped binding name - before that architecture can be built. It is
recorded here rather than fixed now because it is not on the Apex path, and rule 17
requires the fact to be visible, not the fix to be immediate.

## 6. What removal must still pass

Before any C++ is deleted, all of the following must hold:

1. Apex is accepted (rule 15).
2. Every shared module above is still architecture-neutral - no Apex names, no
   Apex shapes, no Apex-only conditionals.
3. Every unported architecture still has what it needs: the substrate loader, the
   graph primitives, the metadata and quantisation modules.
4. `tools/build-arch-unit.sh` still builds each ported architecture from the same
   shared list.
5. Igor's manual acceptance.

Point 3 is the one that constrains deletion hardest: the 138 architectures without
a Goose builder depend on the C++ model code, so removing it is not a matter of
Apex readiness at all.
