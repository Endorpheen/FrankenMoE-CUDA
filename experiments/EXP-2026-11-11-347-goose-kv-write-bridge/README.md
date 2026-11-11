# EXP347: State-write adapter in Goose

The bridge that turns the gated `state_write` decision (EXP345) into something llama can call:
the journal of the module translated back into per-stream cell counts and coalesced ranges.

## Verdict

**ADAPTER GATED, NOT YET CALLED** — this is step 1 of
`docs/goose-migration/integration-readiness-2026-11-11.md`, not part of the accepted Apex path.
It adds no scoped credit on its own.

## What crosses the boundary

Scalars and flat arrays only: the cell positions and sequence masks are widened to `int64` on
this side, the decision runs in Goose, and the journal comes back. The cell struct, the
sequence set and the IO object never leave llama.

## What the adapter had to learn

The module reports a refusal — a mirrored cache, or a partial-only write of a non-SWA cache —
by **writing nothing at all**. Both exits return the same code, so the only signal is an empty
journal. The adapter's first version looked for a return code and therefore turned every refusal
into "zero ranges", which would have written an empty section instead of skipping the write.

## Gate

| check | result |
|---|---|
| 3 000 draws, all four masking types, refusals included | 15 342 checks / 0 failures |
| enabled by default, disabled only by `GGML_GOOSE_KV_STATE_WRITE=0` | 1 check |

| mutation | caught |
|---|---|
| a refusal is reported as success | yes |
| cell counts are not exported | yes |
| ranges are exported with swapped ends | yes |
| the sequence mask is dropped | yes |

## Integration check (no model run)

`integration/llama-kv-cache.cpp` is the working-tree file with the selection block replaced by the
bridge call, compiled with the exact flags the working build uses
(`/usr/bin/c++ ... -O3 -DNDEBUG -fPIC -Wall -Wextra -Wpedantic ...`). `check_integration.py` proves
three things without touching a model:

| check | result |
|---|---|
| compiles clean (no warnings) | yes |
| defined-symbol set unchanged | 418 = 418 |
| only new undefined dependencies | `fg345w_enabled`, `fg345w_select` |

The native block is still in the file behind the same guard, so `GGML_GOOSE_KV_STATE_WRITE=0`
restores it. A cell count above 64 sequences keeps the native path, because the bridge carries the
sequence set in a 64-bit mask.

Stage link (done, no model):

| check | result |
|---|---|
| link with every object of the working build, one swapped | 4 831 552 bytes |
| exported symbols preserved | 5 249 = 5 249, none lost |
| `dlopen` + bridge call from outside llama | ok, `ranges=1 count=2 first=[0,2)` |
| witness line | `FG345W_KV_STATE_WRITE calls=1` |

The stage is a separate file under the ignored `build/`; the working build and `runtime/exp244` are
untouched. One extra symbol is exported by the module itself (`fg345w_generated_main`); it is the
generated entry point and is harmless, but worth knowing if a symbol audit is ever exact-match.

## Apex run on the stage (one run, fixed profile 32K/128)

Owner authorization 2026-11-11 removed the per-run approval gate for periodic validation after a
large integration step. This is that validation, on the agreed profile and the accepted criteria.

| check | result |
|---|---|
| status | RESPONSE_RECEIVED, 128 tokens |
| content hash | `e68df05b...` — equals the accepted EXP244 control |
| tokens | equal, 128 / 128 |
| saved probabilities | equal, 128 / 128 |
| acceptance script | ACCEPTED — prefill 2588.9 tok/s, decode 25.88 tok/s, floors met |
| wall | 17.58 s (single run, no parallel work) |
| `FG345W` witness | **0 calls** — expected: `state_write` is not on the completion path |

So the stage neither changes nor exercises anything measurable here: it proves the substitution is
sound (identical object set, identical outputs) and proves nothing about the module being live. A
non-zero witness needs a route that a completion actually calls — the next integration step.

## Files

- `source/bridge.c` — adapter plus the generated module it includes.
- `integration/llama-kv-cache.cpp` — the call site, compiled but not linked.
- `tests/adaptor.cpp` — comparison driver.
- `test_local.py`, `mutate.py`, `check_integration.py` — adaptor gate, mutations, call-site check.
