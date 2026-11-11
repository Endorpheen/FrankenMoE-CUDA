# EXP369: the disable switches return the candidate to the native line

Every Goose route in the stage has a switch that exists only to turn it off. This run proves that
turning all of them off restores the native behaviour exactly.

## Verdict

**DISABLE CONTRACT HOLDS.**

## Setup

The stage of EXP368 (batch selection, `ubatch_add`, KV write, KV clear), run with

    GGML_GOOSE_UBATCH_ADD=0 GGML_GOOSE_SPLIT_SELECT=0
    GGML_GOOSE_KV_STATE_WRITE=0 GGML_GOOSE_KV_STATE_CLEAR=0

## Result

| check | result |
|---|---|
| completion | `e68df05b…` — same as with the routes enabled |
| save | works, 387 989 920-byte file |
| truncated restore | rejected (`HTTPError`) |
| `FG349S` / `FG350S` / `FG345W` / `FG367S` | **0 occurrences** |
| exit status | server stopped cleanly |

So the switches do what the repository rule says they do: a build with a feature enabled by default
can be returned to the native path without rebuilding, and the answer does not depend on it.

## What it also shows

The two KV routes and the two batch routes are independent: all four can be disabled together while
save, restore and completion keep working, which is the property a rollback would rely on.
