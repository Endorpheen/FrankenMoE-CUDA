# P8-01 closure: server startup, configuration and lifecycle

Criterion: "Startup/shutdown/config/model selection/errors/signals handled by
Goose; health and lifecycle endpoints compatible."

## What is Goose

| area | evidence |
|---|---|
| startup and configuration decisions | EXP445: router mode, embedding batch clamp, auto `n_parallel` with `kv_unified`, `--kv-unified-per-slot` sizing, alias fallback, `n_ctx_slot`; 69 132 cases, 6/6 mutations |
| signal and shutdown policy | EXP448: first interrupt runs the handler, second forces `exit(1)`, ordered shutdown plans per mode, exit-code policy; 340 cases, 8/8 mutations |
| pre-routing, readiness and auth | EXP446: CORS modes, Origin parsing, OPTIONS, 503 loading gate with the frontend exception, API-key gate; 352 665 cases, 7/7 |
| request errors | EXP450: exception classification and error code/status mapping; 64 cases, 6/6 |
| route table | EXP449: 53 routes, custom `/slots/:id_slot`, public health; 429 cases, 6/6 |
| `/props` payload | EXP456: 19-key order plus conditional `connect_code`, cached-vs-live source, 501 when disabled; 111 cases, 10/10 |
| configuration surface parity | EXP350: help, invalid args, flag-vs-env precedence, verbosity, `/props`, training-context warning |

## Live evidence on the Apex profile

Binary `runtime/exp421/unified/llama-server`, Apex-35B-V1.0-Q4_K_M, context
262144, `-ngl all --cpu-moe -fit off`, `--moe-cache-mib 3584`, port 18093.

| run | action | result |
|---|---|---|
| 1 | wait for `/health`, one SIGINT | ready in 5 s, `/health` `{"status":"ok"}`, **exit 0**, cleanup logged, no leftover process |
| 2 | wait for `/health`, two SIGINTs | **exit 1**, `second interrupt` message present, no leftover process |
| 3 | lifecycle endpoints | **503 while loading observed**, then `/health` 200, `/props` with **19 keys** (`default_generation_settings`, `total_slots`, `is_sleeping`, `cors_proxy_enabled` present), `/metrics` 501 in this profile, SIGINT → exit 0 |

These runs match the EXP448 policy and the EXP446 readiness rule: the second
interrupt bypasses the graceful path and exits with code 1.

## Explicit boundaries

Kept native by design, with their own tracked homes:

- `common_params_parse` internals: behaviour is qualified by EXP350 and P7-10;
  re-implementing the vendored parser is not part of the server lifecycle port.
- `--connect` tunnel and the child/monitor thread: experimental feature, not used
  by the accepted Apex profile; it belongs to C2 compatibility.
- model file discovery, HF download and the loader itself: P7-11 and the P7
  loader items.
- OS handler registration (`sigaction`, `SetConsoleCtrlHandler`) and the actual
  stop calls: primitive platform boundary; the policy that decides them is Goose.

## Verdict

P8-01 closes on the Apex server lifecycle scope: the startup, configuration,
routing, readiness, error and signal decisions are Goose with gates and mutation
evidence, and the live profile confirms the two documented shutdown paths and the
lifecycle endpoints. The boundaries above are named and tracked, not silently
dropped.
