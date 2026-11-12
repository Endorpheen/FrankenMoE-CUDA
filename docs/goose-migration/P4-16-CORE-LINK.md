# P4-16: the candidate GGML core link, object by object

Criterion: "Candidate GGML core link contains only Goose-generated logic plus declared primitives; all API symbols..."

The link command is the recorded `scheduler-base-link` step in the EXP164 receipts; it names thirteen objects. Each was classified by the
same mechanical marker used for P2-18 -- `gg_function_N` symbols exist only in Goose-emitted objects -- plus a count of hook references
for the hand-written pieces.

| class | count | objects |
|---|---|---|
| Goose-emitted | 6 | `shared-core` (26 generated functions, 107 hook references), `allocator` (26, 46), `scheduler-goose` (22, 54), `buffers-goose` (9, 38), `stream-goose` (8, 28), `backend-base-goose` (4, 6) |
| hand-written replacement | 1 | `backend-scheduler.o`, compiled from `backend-scheduler.cpp`; the object is rebuilt on demand and is not currently on disk |
| integration shim | 1 | `shared-meta.o` -- no generated functions, three hook references |
| declared native primitives | 5 | `base-ggmlcpp` (ggml.c compiled as C++), `base-opt`, `base-threading`, `base-quants`, `base-gguf` |

The five native objects are the primitives already on record: `ggml.c`'s geometry and allocation core, the optimizer, the thread pool,
the quantizer reference code, and the GGUF parser that P7-01 keeps native deliberately.

## API symbols

The symbol-level parity measured for P4-03 covers the same surface: `libggml-base` exports 550 `ggml_*` symbols in flint and 551 in ours,
the single difference being the additive `gg_backend_sched_set_expert_pipeline`; `libllama` 255 against 255; `libggml-cpu` 6 against 6.
No public symbol is missing.

## Verdict

Every object in the core link is Goose-emitted, a named replacement or shim, or a declared native primitive, and every API symbol still
exists. The two caveats are recorded elsewhere and stand: the deployed base cannot be reproduced from source (P6-02), and signature-level
compatibility is not proven by a symbol table.
