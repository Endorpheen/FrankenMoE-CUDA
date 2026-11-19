# P8-06: model management - two doors served, eleven composed only, and a source/binary drift

Checked 2026-11-19 on `main`, text level, no build.

## The module

`goose/server/model-manager.goose` exports **13** doors:

```
mg_status_str mg_source_str mg_pred mg_unload_action mg_remove_gate mg_stop_gate mg_wait_pred
mg_update_fields mg_arch_take mg_capacity mg_victim mg_tick mg_lru_log
```

## Who calls them

In the shipped server source (`vendor/flint/tools/server/server-models.cpp`) exactly **two** doors
are reached, both through `dlopen`:

- `mg_stop_gate` and `mg_stop_door`, in `server_models::request_stop`: the ported verdict answers,
  the native rule is computed alongside, and the two are compared under `FG_MODEL_STATUS_DIAG`
  with the marker `FG_MODEL_STOP ported calls=… disagreements=…`.

`mg_victim` is called from `integration/m1/common/fg-goose-model-bridge.cpp`, which is the m1
harness, not the shipped server. The other ten doors have no caller at all:
`mg_status_str`, `mg_source_str`, `mg_pred`, `mg_unload_action`, `mg_remove_gate`, `mg_wait_pred`,
`mg_update_fields`, `mg_arch_take`, `mg_capacity`, `mg_tick`, `mg_lru_log`.

## Correction: there is no drift, and my first reading was wrong

The first version of this note said the current source printed `FG_MODEL_STOP` while the binary's
markers were `FG_MODEL_STATUS`/`FG_MODEL_SOURCE`, and concluded that a rebuild would lose the status
and source reach. That was **wrong**, and the mistake was in the search: `mg_status_str` and
`mg_source_str` are wired in **`server-models.h`** (`server_model_status_to_string`, around lines
60-160), not in `server-models.cpp`, and I had grepped only the `.cpp`.

What the header actually contains:

- `fg_mg_status_door()` dlopens `mg_status_str`; `server_model_status_to_string` calls it, compares
  with `server_model_status_to_string_native` under `FG_MODEL_STATUS_DIAG`, and prints
  `FG_MODEL_STATUS ported calls=… disagreements=…`;
- the same shape for `mg_source_str` with `FG_MODEL_SOURCE`;
- a refusal counter, so a door that answers with something that is not its own length is counted
  instead of hidden.

So four of the thirteen doors are reachable in the current source - status, source, stop gate and
stop door - and the shipped library matches the source on that point. The permanent test
`tests/server/router-status` asserts those markers against the shipped library and is consistent with
both.

That also means the earlier sentence "a rebuild from main today would lose that reach" is false, and
the removal order below does not need a restoration step first.

## What the requirement needs

The lifecycle itself - load, stop, unload, remove, hot reload, LRU eviction, a busy model that must
not be stopped - is P8-06's subject, and the router listing is its only exercised call site today
(`tests/server/router-status`, model tier). Everything else is a table over doors
(`tests/models/model-manager-doors`, fast tier), which is exactly as much as can be claimed without
a router run.

Closing it needs, in this order: wire the lifecycle doors where the router calls them, then the real
router run. Nothing here is claimed as measured, and no door is claimed as served that this source
does not call.

## Second correction, from the tool that now answers this question

`python3 tools/door-callers.py goose/server/model-manager.goose` returns **six** doors with a call
site, not four:

| door | called from | path |
|---|---|---|
| `mg_status_str` | `server-models.h` | shipped server |
| `mg_source_str` | `server-models.h` | shipped server |
| `mg_stop_gate` | `server-models.cpp` | shipped server |
| `mg_stop_door` | `server-models.cpp` | shipped server |
| `mg_pred` | `fg-goose-model-bridge.cpp` | m1 harness |
| `mg_unload_action` | `fg-goose-model-bridge.cpp` | m1 harness |
| `mg_victim` | `fg-goose-model-bridge.cpp` | m1 harness |

Seven have none: `mg_remove_gate`, `mg_wait_pred`, `mg_update_fields`, `mg_arch_take`, `mg_capacity`,
`mg_tick`, `mg_lru_log`.

The third pass on this one record is the argument for the tool: hand-greps produced four, then a
header-corrected re-check produced four again with a different composition, and a mechanical answer
produces six. The rule that follows is in AGENTS.md: a claim about call sites comes from
`tools/door-callers.py`, not from a remembered grep.
