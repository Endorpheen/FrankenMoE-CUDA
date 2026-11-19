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

**`fx286pic.o`: the source is EXP286's test fixture, found after the first pass.** The object carries
the string `mmap_fixture.c`, and that file exists at
`experiments/EXP-2026-11-12-286-goose-mmap-bookkeeping/tests/mmap_fixture.c`. It defines `main`, and the
object's defining symbol is `fg286_fx_main`, so the producer is that source compiled with
`-Dmain=fg286_fx_main`. Rebuilt with the pinned compiler it defines exactly
`fg286_fx_main fg286m_range_update fg286m_should_lock` - the object's three globals - **plus**
`gate_fail` and `gate_st`, which the original carries as local symbols. That difference is a compile
flag or a local edit nobody recorded, and it is not cosmetic: two extra globals named `gate_fail`
collide the moment another object defines one, which is the duplicate-definition failure this project
has already had. The object is therefore reproducible from a tracked source **with one unresolved
visibility difference**, not irreproducible.

Earlier in this document's first version it was called irreproducible because `fg286_fx_main` appears
in no source; the string `mmap_fixture.c` inside the object is what made the difference, and it is worth
recording that the answer was in the artefact all along. `fg286m_range_update` is in
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

## `fx286pic.o`, resolved exactly

The difference was a **localization step**, not a compile flag. Reproduced and compared on 2026-11-19:

```
D=experiments/EXP-2026-11-12-286-goose-mmap-bookkeeping
gcc-13 -O2 -fPIC -Dmain=fg286_fx_main -I$D/source -c $D/tests/mmap_fixture.c -o fx1.o
objcopy --localize-symbol=gate_fail --localize-symbol=gate_st fx1.o fx286pic.o
```

The result's symbol table is the original's: `T fg286_fx_main`, `t gate_fail`, `t gate_st`. The module
source declares `extern void (fg286m_fail)(int64_t)` (`mmap_bookkeeping-functions.c:36`), so the helpers
must stay external for the module to link - which is why making them `static` in the fixture is wrong
(it fails to compile, and that was tried), and why the object was localized after the fact instead.

The localization is not cosmetic: without it the fixture exports two globals named `gate_fail` into the
shared library, and the first other object defining that name is a duplicate-definition link failure.

All eighteen now meet a condition: seventeen through a tracked source, and `slru-runtime.o` as the
compiler's own runtime with its version pinned in `RUNTIME_VERSION.txt`. `fx286pic.o` is the eighteenth,
from EXP286's fixture with `-Dmain=fg286_fx_main`, carrying one unresolved difference in symbol
visibility. The honest state of path B is therefore: every input has a named producer, and one of them
still needs its exact compile flags recorded before a rebuild can be trusted.

## What removing them needs

A build that compiles the 16 from source and passes `tools/verify-candidate.sh` with the same component
list and door count as the checkpoint. That is one build, not eighteen, and it is the same build the
reproduction pipeline already describes. Until it passes, the copies stay.
