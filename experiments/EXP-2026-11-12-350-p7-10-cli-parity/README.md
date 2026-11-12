# EXP350: command-line surface -- help, invalid arguments and flag/env precedence

Model: the tiny `stories260K.gguf`, so the server actually reaches context creation and the effective values are observable in
`load_model: initializing, n_slots = ..., n_ctx_slot = ..., kv_unified = ...` and `llama threadpool init, n_threads = ...`.

## --help and invalid arguments

Detailed in defect #5. Summary: 750 help lines natively, 755 ours, both exiting 0; the five extra lines are witnesses, and one witness is
written mid-line without a leading newline, splitting the "reasoning and/or tool calls (default: disabled)" sentence. Invalid argument:
same exit code 1 and same first line, followed by witnesses on our side.

## Precedence

| case | native | ours |
|---|---|---|
| `LLAMA_ARG_THREADS=5` only | `n_threads = 5` | same |
| `LLAMA_ARG_THREADS=5` + `-t 3` | `n_threads = 3` -- CLI wins | same |
| `LLAMA_ARG_CTX_SIZE=1024` + `-c 512 -np 2` | `n_slots = 2, n_ctx_slot = 256, kv_unified = 'false'` | same |
| `LLAMA_ARG_CTX_SIZE=512 LLAMA_ARG_PARALLEL=2` env only | `n_slots = 4, n_ctx_slot = 512, kv_unified = 'true'` | same |
| `-c 512 -np 2` (no env) | `n_slots = 2, n_ctx_slot = 256` | same |

CLI overrides env, the per-slot context is the context size divided by the slot count, and `kv_unified` flips with the slot count -- all
identical on both lines.

One quirk is worth recording because it surprised me: `LLAMA_ARG_PARALLEL` has no effect in either line (`n_slots` stays 4), so that is not
the variable's name. The parity holds either way -- both lines ignore the same wrong name -- but the matrix should not be read as proof that
the parallel variable works, only as proof that both lines treat it the same way.

## Still open in P7-10

The memory-fit policy needs a model large enough to trigger it; `stories260K` cannot. The wider flag matrix (sampling, grammar, template,
logging precedence) is also still ahead.
