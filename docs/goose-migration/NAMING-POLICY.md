# Naming policy, and the current state of every component

The policy is rule 34 of `AGENTS.md`. This document is its accounting: for each component, the target name and what
still stands between it and that name.

FrankenMoE is the product and the runtime. Goose is the language and the substrate the runtime is being moved onto.
Fried Goose is the compiler.

## Where each component stands

| today | future | what still decides the old name |
|---|---|---|
| `libllama.so.0` | `libgoose-runtime.so` | most of its executable logic is still C++: the model graph, the KV and context machinery, the sampler chain's native bodies, the grammar automaton. The metadata reader, the detokeniser decisions and the token encoder (when served) are the parts already Goose |
| `libllama-server-impl.so` | `libgoose-server.so` | the server's HTTP lifecycle, routing, slots and model management are native C++; only the speculative acceptance decision is a Goose export |
| `libggml.so` | `libgoose-compute.so` | the compute substrate: tensor ops, the CPU and CUDA kernels, quantisation. Substrate is explicitly not counted as debt, and it is the least migrated part |
| `llama-server` | `frankenmoe-server` | it is a front end over a native library, so it inherits that library's state |

## What a rename will look like when it happens

1. the Goose implementation is accepted for that component: the shipped call path reaches the export and the native
   body is dead;
2. the shipped artefact is renamed and the old name becomes a symlink, so every existing script, checkpoint and
   release keeps working;
3. the SONAME is added, not replaced, and the consumer/provider signature check (`tools/check-abi-pairs.py`) passes
   for both names;
4. `tools/check-accepted-components.py` gains an entry for the new name before the old one is dropped;
5. the change is one commit with its own verification, and a rollback that is a symlink swap.

Step 3 and step 4 are the reason a rename is not cosmetic: the gates have to know both names during the transition,
or a rename would quietly turn a checked component into an unchecked one.

## What is explicitly not happening now

No file is renamed, no build is reconfigured, no artefact is rebuilt. A rename that arrives before its component does
is a label on the wrong thing, and the project has already paid once for believing a source change had reached the
binary (EXP-2026-11-19-1298).
