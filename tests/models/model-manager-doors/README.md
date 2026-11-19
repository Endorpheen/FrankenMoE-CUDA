# model-manager-doors

**Requirement:** P8-06 (model management, connectors, hot reload).
**Produced by:** the model-manager door work recorded in `tools/model-manager-doors-check.sh`;
entered the permanent tree on 2026-11-19.
**Tier:** fast - no model, no GPU. Needs the built library; skips (exit 77) without it.

## What it proves

The decisions a model router has to make before it stops, unloads or removes a model are asked of
the shipped library directly, as a table over their inputs:

| door | inputs | expectation |
|------|--------|-------------|
| `mg_status_str` | 0..6 | `downloading, incomplete, downloaded, unloaded, loading, loaded, sleeping` |
| `mg_source_str` | 0..2 | `preset, models_dir, cache` |
| `mg_stop_gate` | mapped, stopping | 1 only for a mapped model with no stop in flight |
| `mg_remove_gate` | mapped, source | 0 accepted, 1 not found, 2 not from the cache |
| `mg_unload_action` | mapped, status | 0 nothing, 1 downloading, 2 running, 3 idle |

Two of these are also reached through the router; the other three decide whether a stop, an unload
or a remove may proceed, and the single-model profile never reaches them. A table is the honest
way to hold them until a running model exercises the call site - which P8-06 still requires.

## Why it is a table and not a router test

P8-06 stays PARTIAL until the lifecycle is exercised on a real router: load, stop, unload, remove,
LRU eviction, a busy model that must not be stopped. This test pins the decisions themselves, so a
port that changes a verdict is caught now rather than during that run.

## Run

```
bash tests/models/model-manager-doors/run.sh
```

Expected: `model manager doors: status=7 source=3 stop=4 remove=4 unload=4 failures=0`, exit 0.
The log records the library path, its SHA-256 and the source revision, because the doors live in
that artefact and a result without it is not evidence about anything.
