# router-status

**Requirement:** P8-06 (model management, connectors, hot reload).
**Produced by:** `tools/router-status-check.sh`; entered the permanent tree on 2026-11-19.
**Tier:** model. It starts a server and loads a router, so it needs
`FG_ALLOW_MODEL_RUN=1` - the explicit permission AGENTS.md rule 1 requires - and skips with exit
77 without it. It also skips when the model, the built server or the free endpoint 9101 is absent.

## What it proves

The single-model Apex profile never reaches the model-status renderer, so it was switched and
unverified. This test exercises the real call site: a router started without `-m` over a models
directory lists every model, and the diagnostic reports whether the ported renderers agree with
the native switches.

Assertions:

- `/models` lists at least one model, so the renderer was actually called;
- the first model's status is `unloaded` (it has not been started);
- its source is `models_dir`, which is the second switched renderer;
- the server log carries `FG_MODEL_STATUS ported` and `FG_MODEL_SOURCE ported` with
  `disagreements=0` for both.

## How it treats the host

`AGENTS.md` rules 21 and 29 apply and are implemented in `run.sh`, not assumed:

- the endpoint is the fixed `127.0.0.1:9101`; a foreign owner is a skip, never a bypass;
- the owner of the port is determined by `ss -ltnp` (by pid), not by process name;
- the server is started as a direct child, so `$!` really is its pid - the first version of this
  script used `setsid`, the cleanup killed the wrong process and left a server holding 9101. That
  is recorded here because it happened;
- cleanup stops that pid, waits, escalates to `kill -9` by pid only, and then verifies the port
  was released.

## Run

```
FG_ALLOW_MODEL_RUN=1 bash tests/server/router-status/run.sh
```

Expected: `PASS: the ported status names the model and agrees with the native switch`, exit 0.
The log and `models.json` land in `build/<timestamp>/` with the library SHA-256, the source
revision and the model path.
