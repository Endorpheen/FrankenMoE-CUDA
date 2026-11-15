# Model management, connectors and hot reload (P8-06)

Three independent Goose slices of the router half of `llama-server`:

| slice | module | native source | prefix |
|---|---|---|---|
| model manager | `source/model-manager.goose` | `tools/server/server-models.{cpp,h}` | `mg_` |
| hot reload | `source/hot-reload.goose` | `tools/server/server-models.cpp` (`load_models`) | `hr_` |
| connectors | `source/connectors.goose` | `tools/server/server-connect.cpp`, `server-models.cpp` (source and sidecars) | `cn_` |

Base commit `9981c403`. Nothing here starts a model, a child process or a thread,
and nothing touches the working runtime: every decision is a function of data
the caller already has (rows, status codes, option strings, directory lists).

## What the slices own

### `model-manager` — the state of the manager

The mapping is a table: one row per name with a status, an active-request count,
a last-used stamp and two marks (stopping, wanted by a queue entry).

| export | native anchor | decision |
|---|---|---|
| `mg_status_str`, `mg_source_str` | `server-models.h` 41-63 | the names `/v1/models` and the SSE stream spell |
| `mg_pred` | `server-models.h` 105-124 | `is_ready`, `is_running`, `is_ready_or_sleep`, `is_failed` |
| `mg_unload_action` | `unload` 1518-1543 | cancel a download / stop a child with the exit command / force-kill one still loading / do nothing |
| `mg_remove_gate` | `remove` 1668-1712 | the two refusals: not mapped, not from the cache |
| `mg_wait_pred` | `unload` 1534-1538, `unload_lru` 1309-1315, `remove` 1695-1698 | the three wait predicates |
| `mg_stop_gate` | `request_stop` 1487-1494 | ask the monitor only once per name |
| `mg_update_fields` | `update_status` 1571-1629 | the fields written, when the stop mark is dropped, which members the SSE object carries (sent even for an unmapped name) |
| `mg_arch_take` | `update_status` 1583-1604 | a modality list replaces the current one only when it is an array of strings |
| `mg_capacity`, `mg_victim`, `mg_tick` | `server_lru_sched` 267-272, 274-295, 356-393 | a free slot, the LRU victim over the name-sorted mapping, and the eviction loop with its `n_free < n_needed` budget |
| `mg_lru_log` | `unload_lru` 1295-1318 | the line the operator sees when the limit is reached |

### `hot-reload` — the reload reconciliation

| export | native anchor | decision |
|---|---|---|
| `hr_preset_norm` | 1026-1034, compared at 1113 | two presets differ only if they differ after HOST/PORT/ALIAS/LOG_COLORS are stripped |
| `hr_unload_set` | 1091-1118 | running models the reload takes down, with the reason (source dropped it, preset changed) |
| `hr_erase_set` | 1120-1133 | entries the reload erases: a finished download, a name no source lists, never a download in flight |
| `hr_alias_apply` | 1143-1166 | the alias pass, including the fact that a row sees the new aliases of the rows before it and the old ones of the rows after it |
| `hr_tags` | 1167-1180 | the tag set of a preset |
| `hr_truthy`, `hr_startup_pick`, `hr_startup` | `common/arg.cpp` 1337, 1067-1086 | the load-on-startup list and the `models_max` refusal |
| `hr_hidden`, `hr_hidden_log` | 944-975 | the dedup-cache hiding rule and its log line |

### `connectors` — the tunnel and the model source

| export | native anchor | decision |
|---|---|---|
| `cn_normalize_code`, `cn_format_code`, `cn_resolve_code` | `server-connect.cpp` 60-78, 47-58, 190-192 | the share code: pasted back with whitespace, checked against the 54-byte alphabet at exactly 40 bytes, printed in blocks of eight |
| `cn_upstream_host` | 111-129 | what llama-connect dials: `.sock` skipped, `0.0.0.0` and `::` on loopback, an IPv6 literal bracketed |
| `cn_unavailable` | 167-188 | the four refusal reasons in their native order |
| `cn_find_binary` | 136-165 | the search order: the sibling copy first, then PATH, an empty entry skipped |
| `cn_log_tag` | 80-109 | the child log level, TRACE folded into DEBUG, an untagged banner line as info |
| `cn_model_hf_repo` | `server-models.cpp` 692-698 | which HF repo a model resolves from |
| `cn_sidecars` | 655-678 | the sidecar set: mmproj by default, an unknown kind refused with the list |
| `cn_sidecar_env` | 1431-1444 | the `LLAMA_ARG_SPEC_TYPE` and `LLAMA_ARG_MMPROJ_AUTO` a download child gets |

## Gate

```
bash tests/build.sh          # -O1
bash tests/build.sh o0       # the same gate with the modules at -O0
```

```
FG634_MG cases=22 values=290 failures=0
FG634_HR cases=14 values=98 failures=0
FG634_CN cases=13 values=133 failures=0
```

49 cases, 521 values, 0 failures, byte-identical gate lines at `-O1` and `-O0`
(`tests/gate-O1.log`, `tests/gate-O0.log`).

Each case runs twice: once through the Goose export, once through a transcription
of the native code that carries the line numbers it copies. Where a value is a
contract rather than an implementation detail (status strings, the tunnel texts,
an eviction order), a golden is asserted on the port side as well, so a
transcription slip cannot hide behind an agreeing pair.

The transcription is not the native binary: `server-models.cpp` links the whole
server, and the parts this slice does not own (mutexes, condition variables, the
child monitor, `common_download_*`) have no place in a gate that must not spawn
anything. The transcribed pieces are the branch conditions, the predicates, the
loop budgets and the strings, each cited.

### Cases

`model-manager` (22): status and source names, the four predicates over every
status and two exit codes, the unload action table for mapped and unmapped rows,
the remove gates, the three wait predicates, the stop gate, eight `update_status`
shapes, twelve architecture-merge combinations, the capacity table, five victim
walks (LRU by stamp, a tie that must keep the first name, busy/loading/stopping/
wanted rows skipped, nothing evictable, an empty mapping), five tick scenarios
(two evictions, a fully busy mapping, no queue, no limit, a claimed slot) and the
LRU log line.

`hot-reload` (14): preset normalisation over six option sets, one reload diff with
five rows (removed, changed, unchanged, not running, loading), one erase pass with
all five status shapes, six alias passes (name clash, clash with an alias taken by
an earlier row, clash with an alias a later row still holds, a running row that
keeps its aliases but still claims them, an alias equal to the model's own name,
whitespace and empty entries), tags over six strings, the truthy vocabulary, the
startup list and its guard over twelve combinations, and the dedup hiding rule
with its log.

`connectors` (13): nine share codes (valid, spaced, padded, short, long, an `I`,
a `0`, empty, room code only), the block format, the resolve verdicts, nine host
lists, eight refusal states (including two that carry several problems at once,
which pins the order), six binary searches, nine child log lines, six HF-repo
resolutions, eight sidecar lists (default, explicit, unknown kind, untrimmed
kind, empty, all five, reversed), six sidecar environments.

### Mutations

```
python3 tests/mutate.py 0 6     # 6/6 caught
python3 tests/mutate.py 6 12    # 6/6 caught
```

Twelve semantic edits, module rebuilt at `-O0` for each one; a mutation that
breaks the build counts as caught. `tests/mutations.log` holds the run.

| # | slice | mutation | caught by |
|---|---|---|---|
| 0 | mg | a sleeping child stops counting as running | `pred`, `unload_action` |
| 1 | mg | unloading a loading model no longer force-kills it | `unload_golden` |
| 2 | mg | the LRU walk takes the last of equal timestamps | `victim_tie` |
| 3 | mg | the SSE status change always carries `exit_code` | `update_fields` |
| 4 | hr | the preset comparison keeps the alias option | `preset_norm`, `unload_set` |
| 5 | hr | a finished download is never erased | `erase_set` |
| 6 | hr | an alias held by another model no longer clashes | `alias_backward_clash` |
| 7 | hr | the startup guard refuses a list that fits | `startup_guard` |
| 8 | cn | a pasted share code keeps its whitespace | `normalize` |
| 9 | cn | a wildcard IPv6 bind has no loopback address | `upstream_host` |
| 10 | cn | the tunnel reasons lose their order | `unavailable_reason` |
| 11 | cn | the mmproj sidecar no longer turns MMPROJ_AUTO on | `sidecar_env` |

## Files

| path | what |
|---|---|
| `source/model-manager.goose` | the manager slice, 13 exports |
| `source/hot-reload.goose` | the reload slice, 10 exports |
| `source/connectors.goose` | the connectors slice, 10 exports |
| `tests/common.h` | gate scaffolding: comparison, buffers, name tables |
| `tests/gate_model-manager.cpp` | gate + native transcription for the manager |
| `tests/gate_hot-reload.cpp` | gate + native transcription for the reload |
| `tests/gate_connectors.cpp` | gate + native transcription for the connectors |
| `tests/build.sh` | build and run; `final` (-O1), `o0` (-O0), `mutate` (no cache) |
| `tests/mutate.py` | the twelve mutations, `mutate.py [lo hi]` |
| `tests/checks.sh` | typecheck, composition and collision scan (no gate build) |
| `tests/gate-O1.log`, `tests/gate-O0.log`, `tests/mutations.log`, `tests/checks.log` | recorded runs |
| `build/` | generated C, objects, gate binaries, the composed unit — reproducible, never committed |

## Integration path

The three modules are written to be one unit together, and they already are
one unit together: concatenating them under a single `import std;` and running
`goose check` typechecks (206 specializations), so the router can be composed
with the accepted server modules without a rename pass.

1. Copy the three files to `goose/server/` (`model-manager.goose`,
   `hot-reload.goose`, `connectors.goose`), add one `goose/MANIFEST.json` entry
   per file whose `native` target names the anchors above, and add them to the
   `server` set of `tools/check-composition.sh`.
2. Compose the server unit with `tools/compose-goose.py` next to
   `mcp-core`, `mcp-stdio`, `decision-core`, `server-schema` and `tools-pure`.
   The private helper prefixes (`mg_`, `hr_`, `cn_`) are disjoint from all 93
   declarations this side and from every declaration in the canonical tree, so
   no entry in the compositor's `RENAMES` map is needed.
3. Add one thin C++ wrapper unit (the natural home is
   `runtime/core/src/engine/model_router.cpp`): it builds `sl_u8`/`sl_i64` from
   `std::string`/`std::vector` and returns native types. The wrappers hold no
   state — `server_models` stays the single owner of the mapping.
4. Replace the decision points in place, in this order, leaving every side effect
   and every lock at the call site:
   - `server_lru_sched::has_capacity`, `pick_victim`, `tick` become
     `mg_capacity`, `mg_victim`, `mg_tick`; the caller keeps `models.mutex` and
     performs the `request_stop(victim)` that the returned victim list names.
   - `server_models::unload` takes `mg_unload_action` and still calls
     `request_exit`, `subproc->terminate` and `request_stop(send_exit)` itself, in
     that order; `mg_wait_pred(0, …)` is its wait.
   - `server_models::remove` takes `mg_remove_gate` and `mg_wait_pred(2, …)`.
   - `server_models::request_stop` takes `mg_stop_gate` before the set insert.
   - `server_models::update_status` takes `mg_update_fields` and
     `mg_arch_take`, then builds the SSE object from the returned member flags.
   - `server_models::load_models` takes `hr_unload_set`, `hr_erase_set`,
     `hr_alias_apply`, `hr_tags`, `hr_startup_pick`, `hr_startup`, `hr_hidden`
     and `hr_hidden_log`; `hr_preset_norm` replaces
     `preset_options_for_compare`'s comparison. The lock/wait sequence of the
     reload branch (unlock around `unload`, then the condition wait) stays as it
     is: the exports answer questions, they do not park a thread.
   - `server_connect` takes `cn_normalize_code`, `cn_format_code`,
     `cn_resolve_code`, `cn_upstream_host`, `cn_unavailable`, `cn_find_binary`
     and `cn_log_tag`; `start()`/`stop()` keep the subprocess, the pipe and the
     join.
   - `model_hf_repo`, `parse_sidecars`/`check_sidecar` and the download-child
     environment take `cn_model_hf_repo`, `cn_sidecars` and `cn_sidecar_env`.
5. Re-run `tools/check-composition.sh` (`ok server`), `goose check` on the
   composed unit, and the three gates at `-O1` and `-O0`.

The gates are the compatibility check for step 4: they compare the exact values
the replaced branches produce, so a wrapper that swaps two arguments or drops a
flag shows up immediately.

## Assumptions and limits

- **Names are never empty.** `mg_victim` uses "row < 0" for "no victim" where the
  native code tests `victim.empty()`. An empty model name would diverge.
- **Rows are sorted by the port.** Every walk the native code takes over
  `std::map` is reproduced by a byte-order sort of the row indices; the gate
  feeds unsorted rows on purpose.
- **The alias universe** is the byte-sorted set of all model names plus every
  alias any preset asks for, at most 60 entries (the sets are `i64` bitmasks).
  An alias outside that set is ignored; the native code has no such restriction.
- **Presets are `KEY=VALUE` lines.** Keys are unique and in byte order, which is
  what `std::map` iterates. A value containing a newline cannot be represented.
- **Sidecar lists are comma-joined kinds.** A kind containing a comma cannot be
  represented; the native JSON array could carry one.
- **The clock, the mutexes, the condition variables, the monitor thread and the
  subprocess** are the host's. `mg_tick` returns the victims and sets the stop
  marks; it does not wait for anything.
- **`update_status` quirk kept as found**: the READY branch passes `{}` for
  progress, which is a null `json`, not an empty object, so the stale progress
  info is *not* reset despite the comment. The port encodes the real behaviour
  (the SSE object carries progress only when the caller passes a non-null value).
- **Alias sequencing is deliberate.** The native pass rewrites rows in name order
  and consults the live sets, so an earlier row sees the new aliases of the rows
  before it and the old ones of the rows after it. `alias_forward_clash` and
  `alias_backward_clash` pin both directions; "simplifying" this to a two-phase
  update would change which aliases survive a reload.
- **Windows branches** (`SearchPathW`, `PIPE_NOWAIT`, `CreateProcess`) are not
  modelled; neither are the `llama-connect` child itself, the SSE writer, the
  HTTP handlers or `target/alias` resolution beyond `get_meta`.
- The `-O0` run differs only in the object files; the gate output is identical
  line by line.

## SHA-256

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

The composed unit `build/composed.goose` (SHA-256
`cb91985ad1ba7197404db18d450fade7d66bcdb4d491947e10ca4eb3bc34b766`, goose runtime
`1e714e6146bc`) is reproducible with `bash tests/checks.sh`, which also runs
`goose check` per module and scans all 94 declarations in this package against
the 7669 in the canonical tree (0 collisions).
