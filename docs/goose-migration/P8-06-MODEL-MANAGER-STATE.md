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

## The drift this exposed

The permanent test `tests/server/router-status` asserts, against the **shipped library**, the
markers `FG_MODEL_STATUS ported` and `FG_MODEL_SOURCE ported`, and it passed when it ran against
`builds/flint-acc`. Those markers are not in the current source: this source prints
`FG_MODEL_STOP`. So the library in `builds/` was built from an earlier `server-models.cpp` in
which the status and source doors were reached, and the current source reaches only the stop pair.

That has two consequences, both worth stating plainly:

1. The shipped binary serves **more** of the model-manager decisions than the current source does.
   A rebuild from `main` today would lose that reach, not gain it - which is the failure mode rule
   33 describes, in the direction nobody expects.
2. `tests/server/router-status` is correct for the shipped library and would fail after such a
   rebuild. Its failure would be true: the reach really would be gone.

## What the requirement needs

The lifecycle itself - load, stop, unload, remove, hot reload, LRU eviction, a busy model that must
not be stopped - is P8-06's subject, and the router listing is its only exercised call site today
(`tests/server/router-status`, model tier). Everything else is a table over doors
(`tests/models/model-manager-doors`, fast tier), which is exactly as much as can be claimed without
a router run.

Closing it needs, in this order: restore in source what the shipped binary already reaches (status
and source), then wire the lifecycle doors where the router calls them, then the real router run.
Nothing here is claimed as measured, and no door is claimed as served that this source does not call.
