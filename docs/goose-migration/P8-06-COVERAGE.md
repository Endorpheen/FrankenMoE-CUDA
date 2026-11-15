# P8-06 — model management, connectors and hot reload

Status: **PARTIAL** — 3 of 3 slices accepted; the router wiring is open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 model manager | status predicates, unload action table (cancel / soft stop / force-kill on LOADING / nothing), remove guards, wait predicates, `request_stop`, `update_status` and SSE fields, modality merge, slot capacity, LRU walk by name order, tick budget | 13 `mg_*` |
| S2 hot reload | preset normalisation (HOST/PORT/ALIAS/LOG_COLORS removed), reload and erase sets, alias pass in real row order, tags, load-on-startup, models_max guard, dedup-cache hiding | 10 `hr_*` |
| S3 connectors | share code normalisation and resolve, dial address, four refusal reasons in native order, binary search order, child log levels, model source choice (HF repo, sidecar, env) | 10 `cn_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-631-p8-06-model-management`.
- Canon: `goose/server/{model-manager,hot-reload,connectors}.goose`.
- Gates: 22/290 + 14/98 + 13/133 values, 0 failures, identical at -O0.
- Mutations: 12/12 in two halves.

## Recorded native behaviour

`update_status` does not reset progress (the READY branch sends `{}`, a null
json); SSE is sent for unmatched names; the alias pass is order-sensitive;
unloading a model in LOADING kills the process without an exit command
(`send_exit = false`), while other states get the soft command first.

## Remaining

Wire `runtime/core/src/engine/model_router.cpp` to `mg_`/`hr_`/`cn_` in order:
`server_lru_sched` -> unload/remove/request_stop/update_status -> load_models
reload -> server_connect -> sidecar/hf-repo.

## Boundaries

Mutexes, condition variables, the monitor and the subprocess stay native: the
exports only answer questions. `ensure_model_ready`, `update_download_progress`,
`proxy_request`, the SSE writer, HTTP handlers, the real child llama-connect and
Windows branches are outside the slices.
