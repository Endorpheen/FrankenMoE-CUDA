# EXP354: Simple batch split wired into the candidate (route probe)

The bridge and call site that would make `split_simple` a live Goose route, plus the probe that shows
whether Apex takes that route at all.

## Verdict

**WIRED, NOT LIVE** — the substitution is sound and the outputs are exact, but the route logs zero
calls on the Apex profile. This is a route fact, not a porting failure.

## What was built

- `source/bridge.c` — widens the `used` flags, calls the EXP351 module, reads the taken indices back
  and reports the updated flags. A capture of zero maps to "no free token", which llama reports as an
  empty ubatch.
- `integration/llama-batch.cpp` — `split_simple` calls the bridge when
  `GGML_GOOSE_BATCH_SPLIT_SIMPLE` is not `0`; the ubatch construction stays native.
- `build_stage.py` — links a stage with two call sites (this one and the EXP347 state-write range
  selection): 4 836 000 bytes, all 5 249 exported symbols preserved.

## Adapter gate

| check | result |
|---|---|
| 4 000 draws over 1–16 tokens, 1–5 size limit, all `used` patterns | 49 824 checks / 0 failures |
| symbol set of the integrated object | 123 = 123, two new undefined dependencies only |

## Apex run on the stage (32K/128)

| check | result |
|---|---|
| status | RESPONSE_RECEIVED, 128 tokens |
| content hash | `e68df05b…` — equals the accepted control |
| tokens / probabilities | equal, 128 / 128 |
| acceptance | prefill 2518.4 tok/s, decode 24.64 tok/s (single run, no parallel work) |
| `FG351S` witness | **0 calls** |

So the stage is correct and `split_simple` is not on the Apex path: with a non-unified attention
cache the splitter takes `split_equal`, not the simple walk. The witness printing had to be moved
from the destructor to the first call — a SIGINT shutdown does not run destructors, and an exit-only
witness is indistinguishable from "never called". Both facts are recorded because either one alone
would mislead: the first as "the port does not work", the second as "the route is dead".

## Next live route

`split_equal` (EXP348 keep-tail block and EXP350 opening block) is the one Apex actually calls; it
needs a journal-to-actions bridge, not just a selector.
