# From 11 doors to a runtime: the chain, each link measured

Path B now carries **1253 of 1256 unit doors** and starts as a server. It still does not answer, and every
remaining step is a named symbol rather than a mystery. This is the chain as it was walked tonight, so the
next attempt starts from here instead of from the beginning.

## Fixed and proven

| step | symptom | cause | fix |
|---|---|---|---|
| 1 | 11 unit doors | the composed unit was **never linked** | link it, and refuse to build without it |
| 2 | `R_X86_64_PC32` against `stderr` | the staged unit is built for an executable | compile a PIC copy from the same generated C |
| 3 | `q35_shape`, `qg_split`, `goose_init` defined twice | extra objects superseded by the unit | skip and name each skipped object |
| 4 | `undefined symbol: llama_goose_register` | `gguf-reader-owner.cpp` was in no consolidated source set | add it to our-src |
| 5 | `undefined symbol: fg950_goose_init` | the adapter calls the unit's init by build.py's define | compile the unit with `-Dgoose_init=fg950_goose_init` |
| 6 | `undefined symbol: goose_init` | the rest of the tree uses the unrenamed name | one-line shim calling through |

After 6 the binary starts and prints its Goose witnesses (`FG270Q`, `FG276T`, …) from a set built entirely
from tracked sources.

## Still open, in order

| step | symptom | cause |
|---|---|---|
| 7 | `undefined symbol: goose_tok_enabled` | the C++ m1 bridges (`integration/m1/common/fg-goose-tokenizer-bridge.cpp` and its six siblings) are compiled by no consolidated set. They are the reason the baseline carries ~100 mangled C++ symbols |
| 8 | five accepted components absent | the carrier and bridge set: `goose-shim-marker`, `goose-dry-apply`, `goose-bulk-bridge`, `gguf-reader-call-path`, `goose-spec-synth` |
| 9 | not measured | after 7 and 8 the four axes decide whether it becomes the baseline |

## What step 7 actually requires

The seven C++ bridges live in `integration/m1/common/`, include `goose-host.h` and generated headers, and
need the module-src include paths that `build-from-tree.sh` already assembles. Adding them is the same shape
as step 4: copy `integration/m1/common/*.cpp` and `*.h` into the tree, compile them with `g++-13 -std=c++17`
alongside `modules/*.cpp`, and let the link fail loudly on the next missing symbol - which is how steps 4 to
6 were each found in minutes rather than hours.

## The number that makes this worth finishing

A build that carries 1253 unit doors from tracked sources, instead of an artefact whose producer nobody
recorded, is the difference between a migration that can be reproduced and one that can only be described.
