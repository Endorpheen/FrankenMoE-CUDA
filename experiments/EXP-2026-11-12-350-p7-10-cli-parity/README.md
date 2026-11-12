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

## Startup log, same invocation

`-m stories260K.gguf -c 256 -fit on --port 0`, timestamps stripped, witness lines removed:

    diff native ours -> 6 lines
    10a11  > I srv load_model: RAM-only prefix checkpoints (SWA): disabled, budget 256.0 MiB
    12c13  < listening on http://127.0.0.1:37139   vs   http://127.0.0.1:33119

So the start-up log matches except for the ephemeral port and **one additive informational line** about RAM-only prefix checkpoints. That
line is ours alone; it is informational, not a behavioural difference, but it is a byte-level divergence in the log stream and is recorded as
such.

Both lines also agree on the device decisions: `layer 0 is assigned to device CUDA0`, `Flash Attention not supported, set to disabled`,
`n_threads = 16` (no env in this run), `n_slots = 4`.

## `-fit` is not observable at this size

`-fit on` produces no fit plan at verbosity 3 with either line on this model, because nothing needs adjusting. The memory-fit policy
therefore still needs a model large enough to force an adjustment, which means a run Igor has to approve first.

## Logging verbosity

| case | native | ours |
|---|---|---|
| `LLAMA_ARG_LOG_VERBOSITY=5` only | `verbosity = 5` | same |
| `LLAMA_ARG_LOG_VERBOSITY=1` only | the info line is suppressed | same |
| `LLAMA_ARG_LOG_VERBOSITY=1` + `-lv 5` | `verbosity = 5` -- CLI wins | same |
| `-lv abc` | `error while handling argument "-lv": stoi` | identical, character for character |

## Template and grammar

`--chat-template-file /nope.jinja` gives `error while handling argument "--chat-template-file": error: failed to ...` on both lines,
identical up to the 70-character window captured.

A malformed `--grammar 'root ::= ['` produced no matching line in either log within the first eight seconds, so that case is
inconclusive as captured rather than equal: the error may surface later, and it is not claimed as parity.

## Sampling precedence and the whole /props payload

Server started with `LLAMA_ARG_TEMP=0.2` and `--temp 0.7`, then `/props` read:

- `default_generation_settings.params` is **fully equal** between the lines, not merely equal on the fields checked. Temperature is
  `0.699999988079071` on both -- the command line won over the environment variable, and even the float's decimal expansion matches.
- Walking the entire `/props` document field by field gives **one** difference in total: `media_marker`, which is a per-process random
  string (`__media_...__`). Everything else -- model metadata, context parameters, the whole chat template text, sampler defaults, the
  grammar/tool configuration -- is identical.

That single random marker is the only thing standing between the two payloads being byte-identical apart from key order.
