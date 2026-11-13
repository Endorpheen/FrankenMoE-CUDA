# EXP445: server startup/config policy in Goose (P8-01 first slice)

The server's startup decisions are plain host arithmetic on `common_params` and
on the loaded model. This experiment moves them into a Goose module so the
policy is owned by the migrated runtime instead of by native C++.

## What was ported

`source/startup.goose`, transcribed from `tools/server/server.cpp` (159-200) and
`server-context.cpp` (`n_ctx_slot`, 4932):

1. router mode = empty model path, repo and docker repo;
2. inside `!is_router_server`: embeddings clamp `n_batch` down to `n_ubatch`;
3. inside `!is_router_server`: `n_parallel < 0` becomes 4 and forces
   `kv_unified = true`;
4. **outside** the router guard: `--kv-unified-per-slot` sizes `n_ctx` when the
   user did not pin `-c` and fit did not pin a minimum context;
5. **outside** the router guard: empty alias falls back to the model name;
6. `n_ctx_slot = min(ctx_seq, kv_unified_per_slot if > 0, n_ctx_train)`.

The ordering matters: steps 4 and 5 run for router and single-model mode alike.
An earlier draft had them inside the router guard; the differential gate would
have caught it, and the final module keeps the native placement.

The output is eight i64 words: `valid, n_batch, n_ubatch, n_parallel,
kv_unified, n_ctx, alias_use, n_ctx_slot`. `valid` mirrors the parse-time
contract (positive batch, non-negative ctx/pool, positive slot context,
positive effective parallel), so an impossible tuple is reported instead of
being silently adjusted.

## Gate

`tests/startup_fixture.c` contains a direct C transcription of the native
statements and compares all eight words against the Goose module.

| check | result |
|---|---|
| exhaustive grid over every branch (model/router x embedding x 3 batch pairs x 5 parallel x 2 kv_unified x 2 ctx x 3 pool x 2 fit-min x 2 alias x 2 name length x 2 ctx_seq x 2 ctx_train) | 69 120 cases |
| boundary corpus (Apex shape, router, fit-pinned ctx, `-c` wins, each invalid guard, large pool) | 12 cases |
| total | **69 132 cases, 553 056 values, 0 failures** |
| ASan + UBSan host build | 0 failures, rc 0 |

Mutations 6/6 caught:

| mutation | failures |
|---|---|
| router guard inverted | 52 236 |
| embedding clamp direction flipped | 13 443 |
| auto parallel 4 -> 8 | 8 066 |
| KV sizing ignores the fit minimum ctx | 18 437 |
| alias fallback accepts an empty name | 17 281 |
| slot cap takes the larger value | 69 128 |

## Scope

This slice owns the startup *decisions*. Not yet ported, and therefore P8-01
stays IN_PROGRESS:

- the OS lifecycle itself: `SIGPIPE`/`SIGINT`/`SIGTERM` handlers, graceful
  shutdown drain and exit codes (host/OS surface, still native);
- argument parsing and error text (`common_params_parse`), qualified by EXP350
  but not moved;
- model loading order, health/lifecycle endpoint wiring and the HTTP server
  control flow;
- `--connect` tunnel availability checks.

## Reproduce

    bash experiments/EXP-2026-11-13-445-p8-01-startup-policy/build_fixture.sh
    python3 experiments/EXP-2026-11-13-445-p8-01-startup-policy/mutate.py
