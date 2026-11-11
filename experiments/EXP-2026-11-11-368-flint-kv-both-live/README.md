# EXP368: KV write and clear live in one candidate

Both KV state call sites — the write range selection and the clear plan — in one stage, together with
the two batch blocks.

## Verdict

**LIVE, EXACT** — both KV witnesses fire on their own paths and the completion is unchanged.

## Stage

44 flint objects with `llama-batch.cpp.o` and `llama-kv-cache.cpp.o` replaced by integrated copies and
four bridges linked: 5 305 536 bytes, ABI complete (the KV object's defined symbols go 438 → 446, all
additions local `.LC` labels).

## Run

| check | result |
|---|---|
| completion | `e68df05b…` — unchanged |
| save | 387 989 920-byte file |
| `FG345W_KV_STATE_WRITE` | live, 1 call (the save) |
| truncated restore | rejected (`HTTPError`) |
| `FG367S_KV_STATE_CLEAR` | live (the failure path) |
| batch witnesses | both live |

## Why these two together

The write is on the save path and the clear is on the failed-restore path; neither is on completion.
Running them together proves they coexist without disturbing each other or the answer, which is the
same property EXP362 showed for the two batch blocks.

## Remaining on the KV side

`state_read_data` is not a candidate under the current format (headers and payloads interleave per
layer, EXP366), the whole-cache `state_clear(seq_id == -1)` branch and the hybrid drop path need a
cancellation or an unload, and the meta/data halves have no separate live route.
