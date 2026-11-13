# What the deployed candidate actually contains, and where the rest lives

Found while checking whether P8-04's stop-string port reaches the shipped libraries. It does not, and neither do most integrations from the
second half of the project. This document records the boundary, because it changes how ledger entries should be read.

## The stage is an old snapshot plus one rebuilt library

`experiments/EXP-2026-11-12-340-p6-03-request-trace/stage` is the build this session's runtime experiments ran against. Its README says
plainly what it is: `libllama` rebuilt with the graph-trace hook and relinked into a stage derived from the accepted runtime. So the stage
inherits `libggml-base`, `libggml-cpu` and `libggml-cuda` from the accepted runtime as it stood when that runtime was produced.

## Bridge census: the stage stops around EXP244

Counting a bridge per component across every library in the stage:

| bridge | experiment | present |
|---|---|---|
| `fg157` context core | 157 | 638 refs |
| `fg195` CUDA VMM pool | 195 | 15 |
| `fg196` CUDA pinned pool | 196 | 20 |
| `fg244` | 244 | 15 |
| `fg235` | 235 | 0 |
| `fg248`, `fg250`, `fg258`, `fg262` | 248-262 | 0 |
| `fg276t` stop strings | 276 | 0 |
| `fg284p` shard prefix | 284 | 0 |
| `fg286m` lazy/mmap | 286 | 0 |
| `fg291b` batch | 291 | 0 |
| `fg310q` Q4_K decoder | 310 | 0 |

So the deployed candidate carries the integrations up to roughly EXP244 and none after it.

## Where the later work lives

In experiment directories, as patched copies of the files plus their own gates:

    experiments/EXP-...-277-goose-stop-strings-live/integration/server-context.cpp
    experiments/EXP-...-291-goose-batch-reject/llama-batch-patched.cpp
    experiments/EXP-...-286-goose-mmap-bookkeeping/llama-model-loader-goose.o

No shared source tree and no built library contains them -- `work/llama.cpp-integration` has nothing, and the Fried-Goose tree has nothing.

## What this means for the ledger

Entries that say "integrated" for work after EXP244 mean *the patch exists and its own gate passes*, not *the shipped runtime contains it*.
That covers, among others, the MoE cache integration (EXP262-265), the server decision ports (EXP268-276), batch continuity (EXP291), the
shard-prefix and mmap work (EXP284, EXP286) and the sixteen decoders (EXP310+). The decoders are the exception in one respect: EXP345
verified them against the native encoder as standalone objects, which is real evidence about the port, just not about the candidate.

The Apex milestone, the restore and the throughput numbers were measured on the older candidate and remain valid for it.

## The obvious next step, and why it needs a word from Igor

Until a build merges the patched sources into a candidate and relinks, the second half of the migration is unproven in the runtime. That
merge is a build, then an Apex run to confirm `e68df05b` still holds or to record a legitimate new baseline, which is exactly the kind of run
that needs agreement first.

## Correction: the module sources exist, the assembly definition does not

A first pass at this looked like a provenance hole -- `fg157` appears in the candidate only inside the modular
`libggml-base-*.so` files, and no `.c` or `.cpp` in this repository defines it. That reading was too pessimistic, and the evidence corrects
it:

    experiments/EXP-2026-11-08-157-goose-context-core/
        context.goose  context-policy.goose  context-generated.c  context-qualified.c  core-api.h
        build.py  build-commands.json

So each composition module is reproducible from its own experiment directory: the `.goose` source, the qualified compiler output, the
bridge, and a recorded build command. `runtime/exp157/` holding only `measured/` and `results/` is a leftover of the measurement step, not a
missing source.

What is genuinely missing is a single build definition that assembles the *current* set of modules into one candidate. Today that set is
implicit: it is whatever the accepted runtime happened to contain, and the later experiments were never folded into it. Rebuilding means
walking the experiment set, applying each module's recorded build command, and relinking -- mechanical, but a project-sized step rather than a
one-liner, and it has to end in an Apex run to check whether the accumulated changes still reproduce `e68df05b`.
