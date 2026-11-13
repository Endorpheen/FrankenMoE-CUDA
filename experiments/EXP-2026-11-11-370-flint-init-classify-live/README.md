# EXP370: five Goose routes live in one candidate

Batch classification, batch selection, `ubatch_add`, KV write and KV clear, each on its own path, in
one stage on the accepted line.

## Verdict

**LIVE, EXACT.**

## Stage

44 flint objects with `llama-batch.cpp.o` and `llama-kv-cache.cpp.o` replaced by integrated copies and
five bridges linked: 5 309 904 bytes, ABI complete.

## Run

| path | witness | count in the log |
|---|---|---:|
| batch classification (`init`) | `FG352S_BATCH_CLASSIFY` | first call + 100 |
| batch selection (`split_equal`) | `FG350S_SPLIT_SELECT` | first + 100 + 200 |
| ubatch assembly (`ubatch_add`) | `FG349S_UBATCH_ADD` | first + 100 |
| KV write (`state_write`) | `FG345W_KV_STATE_WRITE` | 1 (the save) |
| KV clear (`state_clear`) | `FG367S_KV_STATE_CLEAR` | 1 (the rejected restore) |

| check | result |
|---|---|
| completion | `e68df05b…` — unchanged |
| save | 387 989 920-byte file |
| truncated restore | rejected (`HTTPError`) |

## What the counts say

Three routes run on every batch (classification once per batch, selection and assembly per ubatch),
one runs on a save and one on a failed restore. The numbers match that shape: the per-batch routes
count in the hundreds across a 128-token request, the two state routes exactly once.

## Composition note

`init`, `split_equal` and `ubatch_add` live in one object; the KV pair in another. Both objects are
compiled from the same flint sources with independent guards, and the EXP369 test shows all of them
can be switched off without touching the answer.
