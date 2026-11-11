# EXP339: Recurrent state clearing and context steps in Goose

`llama_memory_recurrent::state_clear` and `llama_memory_recurrent_context::{next, apply, get_status}`.

## Verdict

**GATED, NOT INTEGRATED** — the clear order, the early exits and the step rules are exact;
the source tree is read-only.

## Scope transferred

`state_clear`: a negative sequence id clears the whole cache and returns at once — no
per-cell work, no tensors. Otherwise the sequence is removed first, and a **zero-length
section stops there**: the removal still happens, the tensors stay untouched.

The tensors are then cleared per layer in the order recurrent, state, PLE. That is **not**
the order `state_read_data` writes them (recurrent, PLE, state), even though the native
comment says the ranges mirror the write pattern — the ranges mirror, the order does not.
Order only shows in a journal, which is why this gate records it.

`next()`: advances the cursor and answers whether another ubatch exists; a context whose
cursor reaches the end reports the end on that call.

`apply()`: an empty ubatch list means "this is an update", and the recurrent cache never
updates — the status must already be no-update and nothing is touched. Otherwise the current
ubatch is placed.

`get_status()` passes the stored status through.

## Gate

| check | result |
|---|---|
| state_clear, 5 000 draws including negative ids and empty sections | 29 000 checks |
| next, 2 000 draws | 2 000 checks |
| apply and status, 1 500 draws | 380 checks |
| total | 31 380 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| a whole-cache clear also walks the tensors | 1 342 |
| the sequence is not removed before clearing | 9 901 |
| a zero-length section clears the whole row range | 1 023 |
| the state tensor is cleared before the recurrent one | 1 904 |
| null tensors are cleared anyway | 3 223 |
| the cursor does not advance | 3 814 |
| the end of the ubatches is not reported | 1 814 |
| an update is treated as a batch | 1 000 |

All eight are caught. The order mutation is the one that keeps the record stream honest:
swapping two adjacent tensor clears is invisible in the resulting bytes.

## Files

- `source/host.goose` — clearing and the context steps.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
