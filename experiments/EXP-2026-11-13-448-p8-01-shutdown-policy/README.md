# EXP448: server signal and shutdown policy in Goose (P8-01 second slice)

The server's signal path has three owned decisions: what an interrupt does on
the first and the second delivery, which shutdown steps run in which order for
router and single-model mode, and which exit code the process returns. This
experiment moves those decisions into Goose.

## What was ported

`source/lifecycle.goose`, transcribed from `tools/server/server.cpp`
(`signal_handler`, both `shutdown_handler` bodies, both `clean_up` bodies) and
`server-context.cpp` (`terminate -> queue_tasks.terminate`):

1. **Signal dispatch**: the first interrupt runs `shutdown_handler`; the second
   forces `exit(1)` with the "Received second interrupt" message; with no
   handler installed nothing happens (the CLI path calls
   `llama_server_terminate` explicitly).
2. **Ordered shutdown plan**, step ids 1..7:
   - router signal: stop models (`stopping`), mcp shutdown, http stop;
   - single-model signal: mcp shutdown, ctx terminate;
   - router cleanup: session-GC stop, stopping, unload all, mcp shutdown,
     `llama_backend_free`;
   - single-model cleanup: session-GC stop, http stop, ctx terminate,
     mcp shutdown, `llama_backend_free`.
3. **Exit code**: 0 on the normal path, 1 for an HTTP-start failure, model-load
   failure, connect failure or second interrupt.

The OS registration (`sigaction`, `SetConsoleCtrlHandler`) and the stop calls
themselves stay native; the module owns the sequence and the decisions.

## Gate

`tests/lifecycle_fixture.c` transcribes the native bodies and compares the
signal triple, the six-word plan and the exit code.

| corpus | scope |
|---|---|
| exhaustive product | terminating x handler x kind x mode x outcome (2x2x2x2x5) with the plan sanity property |
| repeated-signal state machine | 1..4 deliveries with and without a handler; the flag stays set, so the second interrupt must force exit(1) |
| total | **340 cases, 940 values, 0 failures** |
| ASan + UBSan host build | 0 failures, rc 0 |

The sanity property checks every plan independently of the oracle: known step
ids only, no duplicates, the expected length.

Mutations 8/8 caught: second interrupt runs shutdown instead of exit (52
failures), second interrupt exits 0 (52), router plan terminates ctx instead of
stopping http (20), single-model plan order swapped (40), router cleanup loses
`llama_backend_free` (40), single cleanup stops http before the session GC (60),
exit code inverted (80), no-handler signal still runs shutdown (48).

## Scope

The policy is Goose and gated. Not part of this slice and therefore P8-01 stays
IN_PROGRESS:

- live signal delivery against a running server (needs an approved server run);
- the OS handler registration and masking;
- `--connect` child/monitor thread lifecycle and router `join()` behavior;
- argument parsing and error text, model load order, health/lifecycle endpoint
  wiring.

## Reproduce

    bash experiments/EXP-2026-11-13-448-p8-01-shutdown-policy/build_fixture.sh
    python3 experiments/EXP-2026-11-13-448-p8-01-shutdown-policy/mutate.py
