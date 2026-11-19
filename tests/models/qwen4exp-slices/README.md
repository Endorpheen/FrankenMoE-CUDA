# qwen4exp-slices

**Requirement:** C1-02 - the Qwen4Exp graph and metadata path in the general runtime.
**Produced by:** EXP-2026-11-15-660-c1-02-qwen4exp; moved to the permanent tree on 2026-11-19, and
now built from `goose/` instead of the experiment's copies.
**Tier:** integration - compiles four small modules, no model, no GPU, about a minute for all four.

## What it proves

Each slice is an independent transcription of the native lines named in its module header, so the
gate proves the port's rules, names and ordering - not native numeric execution:

| slice | module | result |
|---|---|---|
| `q4m` | `goose/model/q4m-metadata.goose` | 15 cases / 222 values / 0 failures |
| `q4t` | `goose/model/q4t-tensor-map.goose` | 13 cases / 528 values / 0 failures |
| `q4e` | `goose/model/q4e-expert-layout.goose` | 12 cases / 28 values / 0 failures |
| `join` | `q4t-tensor-map.goose` + `qwen4exp.goose` | 3 cases / 156 values / 0 failures |

43 cases / 934 values, 0 failures. The `join` slice is the integration contract: for all 3 layer
kinds x 51 tensor ids, `q4_shape`'s "this kind owns it" and `q4t_present`'s "this kind declares it"
must agree.

## What running it from goose/ found

Two real defects that the experiment's own copies did not have, because they were built as isolated
sets:

- the graph module's abort is renamed to `fgg_fail` when a unit is composed, so a module that
  imports the graph and calls `fail` fails with "unknown function: fail". `qwen4exp` and `q4e` now
  bind the same primitive under their own name (`q4_abort`).
- a fixture that already defines `fg200g_fail` must not be given a second definition; the runner
  adds the stub only when the fixture has none.

## Run

```
bash tests/models/qwen4exp-slices/run.sh          # all four
bash tests/models/qwen4exp-slices/run.sh q4m      # one slice, a few seconds
```

Exit 0 when every slice matches; 77 when no compiler is available.
