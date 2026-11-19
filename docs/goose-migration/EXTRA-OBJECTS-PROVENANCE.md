# extra-objects: 16 of 18 map to a tracked source, 2 are build products

The last gap named in `PATH-B-REPRODUCTION.md`. `tools/consolidate-runtime-tree.sh` copies 18 hand-built
objects from an experiment tree instead of compiling them. This is what each one is, measured
2026-11-19 by taking the symbols it defines and looking for them in tracked sources.

## Mapped to a tracked source (16)

| object | first defined symbol | tracked source |
|---|---|---|
| `b291.o` | `fg291b_check_continuity` | `runtime/reconstructed/src/llama-batch.cpp` |
| `fx262a.o` | `fg262a_run` | `runtime/reconstructed/src/llama-moe-cache.cpp` |
| `fx263-pic.o` | `fg263c_fill_bytes` | `experiments/EXP-2026-11-12-263-goose-moe-cache-copy-decisions/source/copy_decisions-functions.c` |
| `gates.o` | `fg262a_enabled` | `runtime/reconstructed/src/gates.c` |
| `slru-policy.o` | `slru_plan` | `goose/model/slru-policy-goose.goose` |
| `slru-shim.o` | `slru_fail` | `integration/goose-standalone/slru-runtime-shim.c` |
| `fg-ar.o` | `ar_arch` | `goose/model/arch-registry.goose` (in `_GOOSE_PAIRS`) |
| `fg-ar-bridge.o` | `fg_n01s_count` | `integration/m1/common/fg-arch-registry-bridge.c` |
| `fg-q35.o` | `fg_q35_goose_init` | `goose/model/qwen35-split.goose` + its bridge |
| `fg-qg.o` | `fg_qg_goose_init` | `goose/model/qgate-split.goose` + its bridge |
| `fg-q35-bridge.o` | `fg_q35s_enabled` | `integration/m1/common/fg-apex-plan-bridge.c` |
| `fg-plan-ffi.o` | `fg158_fail` | `integration/goose-standalone/graph-bindings.c` |
| `fg-gr.o` | `fg_graph_reuse_goose_init` | `goose/cpu/graph-reuse.goose` |
| `fg-gr-bridge.o` | `fg_gr_check` | `runtime/reconstructed/src/llama-graph.cpp` |
| `fg-q35s.o` | `fg_qwen35_split_goose_init` | `goose/misc/qwen35-split.goose` |
| `fg-q35s-bridge.o` | `fg_qwen35_split_init` | `runtime/reconstructed/src/unicode.cpp` |

## Not yet mapped (2)

| object | symbols | what it looks like |
|---|---|---|
| `fx286pic.o` | `fg286_fx_main`, `fg286m_range_update`, `fg286m_should_lock` | `fg286m_range_update` **is** defined in `experiments/EXP-2026-11-12-286-goose-mmap-bookkeeping/source/mmap_bookkeeping-functions.c:184`, so the module source exists; `fg286_fx_main` does not appear there, which suggests the object also carries a fixture entry point compiled separately |
| `slru-runtime.o` | 67 symbols, `gs_abort`, `gs_asfail_f`, `gs_asfail_i`, ... | a generated host shim: the `gs_*` names belong to Goose-generated C, and they appear in this tree only inside build outputs, never in a `.goose` or `.c` source |

Both are build products rather than sources, which is why a text search for them finds nothing in the
repository. They are reproducible once the exact producer is named: the mmap-bookkeeping module for the
first, whichever module generates the `gs_*` shim for the second.

## Resolved after the first pass

**`slru-runtime.o` is the Goose C runtime itself, not a module.** The generated C says so:
`GS_API ... gs_abort(int err, const char *file, int line) {` is a *definition* in
`builds/flint-acc/src/goose-sampler-chain/sampler_chain.c:1230`, and the file's own header explains that
in standalone mode "one translation unit holds it all", while in separate mode `goose --emit-runtime`
writes the runtime out as a C file of its own, "compiled once and linked with every program". So the
object is the compiler's runtime, its source is the compiler's output, and the pinned compiler is
`2026.3-string` with `RUNTIME_VERSION 1e714e6146bc`. Producer named, version pinned - it is a
dependency on the compiler, not an unrecorded artefact.

**`fx286pic.o` is the one object that cannot be reproduced today.** `fg286m_range_update` is in
`experiments/EXP-2026-11-12-286-goose-mmap-bookkeeping/source/mmap_bookkeeping-functions.c:184`, but
`fg286_fx_main` appears in no `.c`, `.cpp`, `.goose`, `.py` or `.json` in this repository or in the
external snapshot. What EXP286 does record is the compiler that produced its binaries:
`/home/end0/AI/Fried-Goose/.cache/compiler-exp-handoff/runtime-candidate-v61/stage3`, sha256
`b87af17a...`, which is **not** the pinned `2026.3-string`. So the object is a PIC build of a source
that was never committed, made by a compiler build that is not the one this project pins.

## The rule this establishes

An object in `extra-objects` must satisfy one of two conditions, or it does not belong there:

1. a tracked source defines its symbols, and the build compiles that source instead of copying the
   object;
2. it is a build product, and the command that produces it is recorded next to the object.

Seventeen meet a condition today: sixteen through a tracked source, and `slru-runtime.o` as the
compiler's own runtime with its version pinned in `RUNTIME_VERSION.txt`. One does not: `fx286pic.o`.
The honest state of path B is therefore "reproducible except `fx286pic.o`", which is a much smaller
statement than "except extra-objects".

## What removing them needs

A build that compiles the 16 from source and passes `tools/verify-candidate.sh` with the same component
list and door count as the checkpoint. That is one build, not eighteen, and it is the same build the
reproduction pipeline already describes. Until it passes, the copies stay.
