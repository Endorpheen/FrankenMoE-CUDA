# EXP-2026-11-15-682 — the runtime reads real models, not only fixtures

Base: `ef077057`. File parsing only: no model was started, no GPU was touched, no
inference was run.

## 1. Why this matters more than another layer

Every result so far came from `tiny-hybrid.gguf`-sized fixtures or upstream vocab
files. A runtime that only works on a fixture has not been tested against the
thing it must read. This runs the existing v4 binary against three real model
files of different shapes.

## 2. Results

| model | file size | kv keys | tensors | payload | mismatches |
|---|---:|---:|---:|---:|---:|
| `LFM3-1.7B-Q8_0.gguf` | 1.8 GB | 45 | 287 | 1 837 991 168 B | **0** |
| `Ternary-Bonsai-2-27B-mmproj-Q8_0.gguf` | 601 MB | 26 | 334 | 629 227 328 B | **0** |
| `mmproj-LFM3-Audio-2B-BF16.gguf` | 545 MB | 40 | 319 | 570 823 712 B | **0** |

For `LFM3-1.7B` the payload is the whole file: 1 837 991 168 bytes declared and
1 837 991 168 bytes placed, so nothing was skipped or double-counted.

`agree=yes` on all three: every metadata key was accepted by the Goose table, and
the substrate reader and the Goose table report the same first key.

`mismatches=0` on 940 tensors across three architectures: ggml's own placement of
every tensor equals the offset `goose/model/gguf-offsets.goose` computes. Both
sides derive them independently.

## 3. What this does and does not prove

**Proves**: the standalone runtime's file layer handles real models - real key
sets (26-45 keys), real tensor counts (287-334), real sizes (545 MB - 1.8 GB), and
the Goose layout arithmetic agrees with ggml on every one of 940 tensors.

**Does not prove**: that the runtime can execute a model. The payload is loaded
and addressable; no graph is built from these weights and no forward pass runs.
That is the next layer, and it needs the backend seam plus a per-architecture
graph build.

## 4. One difference worth noting

`mmproj-LFM3-Audio-2B` shows `copied` 24 bytes below `total`. That is the final
alignment padding: the layout rounds the last tensor up to the 32-byte boundary
and the reader does not write the gap. The other two divide evenly, which is why
the difference appears only here - and it is the expected behaviour, not a defect.

## 5. Cost

No new adapter, no new module, no build: the v4 binary already existed and the
three runs used it unchanged. The only output is this record.

## 6. Reproduce

```
./builds/goose-standalone/frankenmoe-v4 <model.gguf>
```

Reads metadata, builds the tensor directory, loads the payload and compares the
layout. It does not start a model.
