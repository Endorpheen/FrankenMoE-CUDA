# P8-06 report — model management, connectors and hot reload

Base commit `9981c403`. Three slices, three Goose modules, three differential
gates, one binary per slice. Nothing was run against a model, a child process or
the working runtime.

## Result

| slice | module | exports | cases | values | failures |
|---|---|---|---|---|---|
| model manager | `source/model-manager.goose` | 13 | 22 | 290 | 0 |
| hot reload | `source/hot-reload.goose` | 10 | 14 | 98 | 0 |
| connectors | `source/connectors.goose` | 10 | 13 | 133 | 0 |
| **total** | | **33** | **49** | **521** | **0** |

- `-O1` and `-O0` produce byte-identical gate lines: `tests/gate-O1.log`,
  `tests/gate-O0.log`.
- Mutations: **12/12 caught**, two halves of six, each rebuilt at `-O0`
  (`tests/mutations.log`).
- `goose check` passes for each module and for the three composed into one unit
  (206 specializations, no rename needed): `tests/checks.log`.
- No declaration collides with the canonical Goose tree (94 declarations in this
  package against 7669 in `goose/**/*.goose`, 0 collisions), so the slices can
  join the server unit as they are.

## What was ported, and why these three cuts

The inventory of `runtime/core/src` and the native server found the router's
decision surface in two files: `tools/server/server-models.cpp` (3081 lines,
almost entirely state and lifecycle) and `tools/server/server-connect.cpp`
(297 lines, pure string policy). `runtime/core/src/engine` has no model manager of
its own — the router *is* the manager — so the port target is the decision layer,
not a runtime module.

The three cuts follow the lifecycle rather than the file layout:

1. **manager state** — what a status *means* and what the manager *does* with it:
   predicates, the unload/remove verdicts, the three waits, the slot arithmetic
   and the eviction order. This is the part the acceptance ("load/unload/swap/
   list lifecycle, pending/active requests and resource cleanup checked without
   changing the working runtime") is really about, because every branch is a
   decision over state the caller already holds.
2. **hot reload** — the reconciliation of a new preset list against a live
   mapping. It is the only place where "which instance goes down, which entry
   disappears, which alias survives" is decided, and it has the subtlest ordering
   in the file.
3. **connectors** — how a model is reached: the `llama-connect` tunnel (share
   code, dial address, refusal order, binary search, log levels) and the model
   source (HF repo, sidecar set, download-child environment).

## Method

Every case runs twice: through the Goose export and through a transcription of
the native code that carries the line numbers it copies. The compared fields are
the values the native functions return or write — never the logging around them.
Contracts that a reviewer would want pinned (status strings, the four tunnel
texts, an eviction order, the alias conflict log) are additionally asserted as
goldens on the port side, so two agreeing mistakes still fail.

Because `server-models.cpp` pulls in the whole server, the transcription is a
copy of the branch conditions, the predicates, the loop budgets and the strings,
not a link against the native binary. That is the same shape the accepted server
slices used (EXP612, EXP623), and it keeps the gate free of models, children,
threads and files.

The gate feeds rows in insertion order and lets the module sort them, so the
`std::map` walk the native code relies on is tested rather than assumed.

## Findings worth carrying into the runtime

1. **`update_status` does not reset progress.** The READY branch passes `{}` for
   `progress`, which constructs a null `json`, so `!args.progress.is_null()` is
   false and the stale download progress survives the load. The comment says
   "reset progress info"; the code does not. The port encodes the real behaviour.
2. **The SSE status object is sent for an unmapped name.** The meta lookup guards
   the writes but not `notify_sse`, so a status update for a name that was just
   erased still reaches every `/models/sse` client.
3. **A reload's alias pass depends on row order.** Rows are rewritten in name
   order against the live sets, so an earlier row sees the new aliases of the rows
   before it and the old ones of the rows after it. Two-directional coverage
   (`alias_forward_clash`, `alias_backward_clash`) is in the gate; a "clean"
   two-phase rewrite would change which aliases survive.
4. **`unload` on a LOADING model force-kills without the exit command**
   (`send_exit = false`), while every other running status gets the graceful
   command first. `unload_all` repeats the same rule.
5. **`pick_victim` treats "no victim" as an empty name.** A model with an empty
   name would be unpickable natively and pickable here; the gate never uses empty
   names and the README records the assumption.

## Files and hashes

```
e2e151e3ee62782dedf5611bd1fa1fba348ccedad0791f44c6658c6bd4cb5f4b  source/connectors.goose
5b9a6d710679a4fedc3ead6355d05005596b8f9c36989251d550e675d6f8b57c  source/hot-reload.goose
4a3eb02d2a30ab564f6998d9a27e5762a454894c0895d0418ba42262ca6915ff  source/model-manager.goose
b7acfebc263f7c3c9f027e57059a671f0c8ec3313c4a8e2888b7fa62aa9c786a  tests/gate_connectors.cpp
dba332c0cb5d5ff1a9517d938c3723dcd893eebc198cb5ed3b37d9bc9ce8f034  tests/gate_hot-reload.cpp
c206f1bca733a329ddc058d2350bb095ae67aa21afdbf68a610766d19c4bd87a  tests/gate_model-manager.cpp
4024a7c3e3dc8dac8a995f5c88f9fdb63397f840dff1e9da983f6d700fa5a992  tests/common.h
0259ae919a5cf3dcfb474694ba9e45ab92ca10b0da23d5bc094a67a1c946898b  tests/build.sh
b8458b1aca79e006d4f84b6e5752d7fc7b5f3b61a3bee98dac12e85ddeec9903  tests/mutate.py
bdcec41cfac6fea1cedbafb180cce4d32e4b4f649fa39819389406e11546e359  tests/checks.sh
```

Logs: `tests/gate-O1.log`, `tests/gate-O0.log`, `tests/mutations.log`,
`tests/checks.log`. Composed unit `build/composed.goose`:
`cb91985ad1ba7197404db18d450fade7d66bcdb4d491947e10ca4eb3bc34b766`
(reproducible with `bash tests/checks.sh`, not part of the package).
`build/` holds the generated C, the objects and the gate binaries; it is
reproducible from the sources above and is not part of the package.

## Limits

The clock, the mutexes, the condition variables, the monitor thread and the
subprocess are the host's: `mg_tick` returns victims and sets stop marks, it does
not wait, and `cn_find_binary` is a list of directories that exist rather than a
`PATH` walk. Model names are assumed non-empty; the alias universe is capped at 60
strings because the sets are `i64` bitmasks; presets are `KEY=VALUE` lines with
unique keys in byte order; sidecar lists are comma-joined. Windows branches,
the real `llama-connect` child, the SSE writer, the HTTP handlers and
`target`/`alias` resolution beyond `get_meta` are outside these slices. The
`ensure_model_ready` wait loop, the download progress bookkeeping
(`update_download_progress`, `unfinished_downloads`) and `proxy_request`'s
request counting remain for the next slice.

## Next step

Compose the three modules into the server unit in the runtime and replace the
decision points in the order listed in `README.md`, starting with
`server_lru_sched` (smallest call surface, three functions) and finishing with
`server_connect`, then re-run `tools/check-composition.sh`, `goose check` and the
two gate builds.
