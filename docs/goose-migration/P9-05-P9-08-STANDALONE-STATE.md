# P9-05 and P9-08: the standalone engine - decisions in Goose, consumers only in the m1 harness

Checked 2026-11-19 on `main`, text level, no build. The two requirements are the same subject seen
from two sides: P9-05 is the session/runtime/draft lifecycle, P9-08 is the standalone engine that
would own it.

## The doors

`goose/misc/runtime-facade.goose` exports six:

```
rts_config   rts_n_batch   rts_ubatch   rts_plan   rts_request   rts_result
```

`expert_bytes_per_slot` - the function P9-05 asks to move into a Goose contract - has no definition
and no call site anywhere in the tree.

## Who calls them

| door | call sites |
|---|---|
| `rts_n_batch`, `rts_ubatch`, `rts_plan` | `integration/m1/common/fg-goose-session-bridge.cpp`, exercised by the gate `integration/m1/gates/s32/fg_session_smoke.cpp` |
| `rts_config`, `rts_request`, `rts_result` | none |

**Nothing in `vendor/flint/src` and nothing in `runtime/` calls any of the six.** That is the point:
`runtime/cli/main.cpp` and the session code the standalone binaries use are the call sites P9-08
names, and they reach none of these decisions.

## What that means

The m1 bridge is a harness, not the shipped engine. So the honest state of both requirements is:
the decisions exist, a harness exercises three of them against its own gate, and the engine that
P9-08 wants closed never asks them. A rebuild or a new standalone binary would still run the native
session path.

## What closing them needs

1. `expert_bytes_per_slot`: find what computes it today (the requirement says it exists on the
   native side) and give it the Goose form plus its caller - both halves, or neither is a port;
2. wire `rts_n_batch` / `rts_ubatch` / `rts_config` into the session creation path of the standalone
   engine, with a drift check on the configuration the engine actually uses;
3. then the real engine run: prefill/decode, draft state, state reuse and request completion, which
   is a model run and stays behind the usual permission.

Nothing here is claimed as wired or measured. The three doors that do answer are answering a
harness gate, and that is recorded as such.
